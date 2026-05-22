import pytest
import ctypes
import struct


# Simulated clipboard data buffer management (mirrors the C logic)
class ClipboardDataBuffer:
    """Simulates the clipboard manager's buffer management logic."""
    
    def __init__(self, allocated_size):
        self.allocated_size = allocated_size
        self.data = bytearray(allocated_size)
        self.length = 0
    
    def safe_append(self, new_data: bytes) -> bool:
        """
        Safely append data to the buffer.
        Returns True if operation is safe, False if it would overflow.
        
        The vulnerable code does:
            memcpy(tdata->data + tdata->length, data, length + 1)
        without checking that tdata->length + length + 1 <= allocated_size
        """
        length = len(new_data)
        # The +1 mirrors the vulnerable code's `length + 1` (null terminator copy)
        required_space = self.length + length + 1
        
        # SECURITY INVARIANT: must always verify bounds before copy
        if required_space > self.allocated_size:
            return False  # Would overflow — reject
        
        # Safe to copy
        self.data[self.length:self.length + length] = new_data
        self.data[self.length + length] = 0  # null terminator
        self.length += length
        return True
    
    def vulnerable_append(self, new_data: bytes):
        """
        Mirrors the VULNERABLE behavior: copies without bounds check.
        Used to demonstrate what the invariant prevents.
        """
        length = len(new_data)
        # This would be the unsafe operation — we simulate detection
        required_space = self.length + length + 1
        return required_space, self.allocated_size


@pytest.mark.parametrize("payload", [
    # Empty payload
    b"",
    # Single byte
    b"A",
    # Exactly fills buffer (allocated=16, length=0, data=15 bytes + null = 16)
    b"A" * 15,
    # One byte over buffer boundary
    b"A" * 16,
    # Massively oversized payload
    b"A" * 1024,
    b"A" * 65535,
    b"A" * (2**20),
    # Null bytes in payload
    b"\x00" * 10,
    b"\x00" * 100,
    # Mixed null and data
    b"AB\x00CD\x00EF",
    # Maximum integer boundary values
    b"X" * 255,
    b"X" * 256,
    b"X" * 257,
    # Clipboard attack payloads (oversized clipboard content)
    b"\xff" * 512,
    b"\x41" * 4096,
    # Format string-like content
    b"%s%s%s%s%s%s%s%s",
    b"%n%n%n%n",
    # Binary data
    bytes(range(256)),
    bytes(range(256)) * 4,
    # Repeated patterns
    b"\xde\xad\xbe\xef" * 100,
    b"\x00\xff" * 200,
    # Unicode-like byte sequences
    b"\xc0\x80" * 50,
    b"\xed\xa0\x80" * 30,
    # Very long single-type payload
    b"clipboard_attack_" * 500,
    # Payload that would cause integer overflow in length+1
    # (simulated — actual 2^32-1 not practical, use large value)
    b"Z" * (2**16 - 1),
    b"Z" * (2**16),
    b"Z" * (2**16 + 1),
])
def test_clipboard_buffer_no_overflow(payload):
    """
    Invariant: The clipboard manager must NEVER write beyond the allocated
    buffer boundary. For any input data of length L, the operation
    memcpy(buf + offset, data, L + 1) must only proceed if
    (offset + L + 1) <= allocated_size. This must hold for ALL inputs,
    including adversarial clipboard content from local X11 applications.
    """
    ALLOCATED_SIZE = 64  # Simulate a fixed-size clipboard buffer
    
    buf = ClipboardDataBuffer(allocated_size=ALLOCATED_SIZE)
    
    length = len(payload)
    # The +1 mirrors the vulnerable code copying length+1 bytes (with null terminator)
    bytes_to_copy = length + 1
    
    # INVARIANT 1: If the operation would overflow, safe_append must reject it
    required = buf.length + bytes_to_copy
    would_overflow = required > ALLOCATED_SIZE
    
    result = buf.safe_append(payload)
    
    if would_overflow:
        # Must be rejected — writing would go out of bounds
        assert result is False, (
            f"SECURITY VIOLATION: Buffer overflow not prevented! "
            f"Payload length={length}, bytes_to_copy={bytes_to_copy}, "
            f"buffer_offset={buf.length}, allocated={ALLOCATED_SIZE}, "
            f"required={required}"
        )
    else:
        # Safe to write — must succeed
        assert result is True, (
            f"Safe write incorrectly rejected: "
            f"length={length}, required={required}, allocated={ALLOCATED_SIZE}"
        )
    
    # INVARIANT 2: Buffer length must never exceed allocated size
    assert buf.length <= ALLOCATED_SIZE, (
        f"SECURITY VIOLATION: buf.length ({buf.length}) exceeds "
        f"allocated_size ({ALLOCATED_SIZE})"
    )
    
    # INVARIANT 3: The vulnerable pattern (no bounds check) must be detectable
    required_space, alloc = buf.vulnerable_append(payload)
    if required_space > alloc:
        # This is the condition the vulnerable code FAILS to check
        # Our invariant requires this check to exist and be enforced
        assert would_overflow, (
            "Inconsistency: vulnerable path detects overflow but safe path does not"
        )


@pytest.mark.parametrize("initial_fill,payload", [
    # Buffer partially filled, then overflow attempted
    (10, b"A" * 60),   # 10 + 60 + 1 = 71 > 64
    (63, b"A"),         # 63 + 1 + 1 = 65 > 64
    (64, b""),          # 64 + 0 + 1 = 65 > 64 (null terminator overflow)
    (32, b"B" * 32),   # 32 + 32 + 1 = 65 > 64
    (0,  b"C" * 63),   # 0 + 63 + 1 = 64 == 64 (exact fit, safe)
    (0,  b"C" * 64),   # 0 + 64 + 1 = 65 > 64 (overflow by null terminator)
    (1,  b"D" * 62),   # 1 + 62 + 1 = 64 == 64 (exact fit, safe)
    (1,  b"D" * 63),   # 1 + 63 + 1 = 65 > 64 (overflow)
    (0,  b"\x00" * 64),# null bytes, overflow
    (50, b"E" * 20),   # 50 + 20 + 1 = 71 > 64
])
def test_clipboard_buffer_with_existing_data(initial_fill, payload):
    """
    Invariant: Even when the buffer already contains data (tdata->length > 0),
    appending new clipboard data must never write beyond the allocated boundary.
    The offset (tdata->length) must be accounted for in all bounds checks.
    This is critical because the vulnerable code uses tdata->length as an offset
    without verifying remaining capacity.
    """
    ALLOCATED_SIZE = 64
    
    buf = ClipboardDataBuffer(allocated_size=ALLOCATED_SIZE)
    
    # Pre-fill the buffer to simulate existing clipboard data
    if initial_fill > 0:
        fill_data = b"X" * min(initial_fill, ALLOCATED_SIZE - 1)
        buf.length = len(fill_data)
        buf.data[:len(fill_data)] = fill_data
    
    length = len(payload)
    bytes_to_copy = length + 1  # +1 for null terminator (mirrors vulnerable code)
    required = buf.length + bytes_to_copy
    would_overflow = required > ALLOCATED_SIZE
    
    result = buf.safe_append(payload)
    
    # INVARIANT: Overflow must always be prevented
    if would_overflow:
        assert result is False, (
            f"SECURITY VIOLATION: Overflow not prevented with existing data! "
            f"initial_fill={initial_fill}, payload_len={length}, "
            f"bytes_to_copy={bytes_to_copy}, required={required}, "
            f"allocated={ALLOCATED_SIZE}"
        )
    
    # INVARIANT: Buffer length must never exceed allocated size after operation
    assert buf.length <= ALLOCATED_SIZE, (
        f"SECURITY VIOLATION: buf.length ({buf.length}) > allocated ({ALLOCATED_SIZE})"
    )
    
    # INVARIANT: No write should have occurred beyond allocated boundary
    # (verified by checking length didn't grow past limit)
    if result is True:
        assert buf.length <= ALLOCATED_SIZE
    else:
        # On rejection, length must be unchanged
        assert buf.length == min(initial_fill, ALLOCATED_SIZE - 1) or buf.length == initial_fill