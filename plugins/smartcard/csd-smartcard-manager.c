/* csd-smartcard-manager.c - object for monitoring smartcard insertion and
 *                           removal events
 *
 * Copyright (C) 2006, 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA
 * 02110-1335, USA.
 *
 * Written By: Ray Strode
 */
#include "config.h"

#include "csd-smartcard-manager.h"

#define SMARTCARD_ENABLE_INTERNAL_API
#include "csd-smartcard.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <prerror.h>
#include <prinit.h>
#include <nss.h>
#include <pk11func.h>
#include <secmod.h>
#include <secerr.h>

#ifndef CSD_SMARTCARD_MANAGER_DRIVER
#define CSD_SMARTCARD_MANAGER_DRIVER LIBDIR"/pkcs11/libcoolkeypk11.so"
#endif

#ifndef CSD_SMARTCARD_MANAGER_NSS_DB
#define CSD_SMARTCARD_MANAGER_NSS_DB SYSCONFDIR"/pki/nssdb"
#endif

#ifndef CSD_MAX_OPEN_FILE_DESCRIPTORS
#define CSD_MAX_OPEN_FILE_DESCRIPTORS 1024
#endif

#ifndef CSD_OPEN_FILE_DESCRIPTORS_DIR
#define CSD_OPEN_FILE_DESCRIPTORS_DIR "/proc/self/fd"
#endif

typedef enum _CsdSmartcardManagerState CsdSmartcardManagerState;
typedef struct _CsdSmartcardManagerWorker CsdSmartcardManagerWorker;

enum _CsdSmartcardManagerState {
        CSD_SMARTCARD_MANAGER_STATE_STOPPED = 0,
        CSD_SMARTCARD_MANAGER_STATE_STARTING,
        CSD_SMARTCARD_MANAGER_STATE_STARTED,
        CSD_SMARTCARD_MANAGER_STATE_STOPPING,
};

struct _CsdSmartcardManagerPrivate {
        CsdSmartcardManagerState state;
        GList        *modules;
        char        *module_path;

        GList        *workers;

        GPid smartcard_event_watcher_pid;
        GHashTable *smartcards;

        guint poll_timeout_id;

        guint32 is_unstoppable : 1;
        guint32 nss_is_loaded : 1;
};

struct _CsdSmartcardManagerWorker {
        CsdSmartcardManager *manager;
        int manager_fd;

        GThread      *thread;
        SECMODModule *module;
        GHashTable *smartcards;
        int fd;
        GSource *event_source;

        guint32 nss_is_loaded : 1;
};

static void csd_smartcard_manager_finalize (GObject *object);
static void csd_smartcard_manager_class_install_signals (CsdSmartcardManagerClass *service_class);
static void csd_smartcard_manager_class_install_properties (CsdSmartcardManagerClass *service_class);
static void csd_smartcard_manager_set_property (GObject       *object,
                                                guint          prop_id,
                                                const GValue  *value,
                                                GParamSpec    *pspec);
static void csd_smartcard_manager_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec);
static void csd_smartcard_manager_set_module_path (CsdSmartcardManager *manager,
                                                   const char          *module_path);
static void csd_smartcard_manager_card_removed_handler (CsdSmartcardManager *manager,
                                                        CsdSmartcard        *card);
static void csd_smartcard_manager_card_inserted_handler (CsdSmartcardManager *manager_class,
                                                         CsdSmartcard        *card);
static gboolean csd_smartcard_manager_stop_now (CsdSmartcardManager *manager);
static void csd_smartcard_manager_queue_stop (CsdSmartcardManager *manager);

static CsdSmartcardManagerWorker *csd_smartcard_manager_create_worker (CsdSmartcardManager  *manager,
                                                                       SECMODModule         *module);

static CsdSmartcardManagerWorker * csd_smartcard_manager_worker_new (CsdSmartcardManager *manager,
                                                                     int                  worker_fd,
                                                                     int                  manager_fd,
                                                                     SECMODModule        *module);
static void csd_smartcard_manager_worker_free (CsdSmartcardManagerWorker *worker);
static gboolean open_pipe (int *write_fd, int *read_fd);
static gboolean read_bytes (int fd, gpointer bytes, gsize num_bytes);
static gboolean write_bytes (int fd, gconstpointer bytes, gsize num_bytes);
static CsdSmartcard *read_smartcard (int fd, SECMODModule *module);
static gboolean write_smartcard (int fd, CsdSmartcard *card);

enum {
        PROP_0 = 0,
        PROP_MODULE_PATH,
        NUMBER_OF_PROPERTIES
};

enum {
        SMARTCARD_INSERTED = 0,
        SMARTCARD_REMOVED,
        ERROR,
        NUMBER_OF_SIGNALS
};

static guint csd_smartcard_manager_signals[NUMBER_OF_SIGNALS];

G_DEFINE_TYPE (CsdSmartcardManager,
               csd_smartcard_manager,
               G_TYPE_OBJECT);

static void
csd_smartcard_manager_class_init (CsdSmartcardManagerClass *manager_class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (manager_class);

        gobject_class->finalize = csd_smartcard_manager_finalize;

        csd_smartcard_manager_class_install_signals (manager_class);
        csd_smartcard_manager_class_install_properties (manager_class);

        g_type_class_add_private (manager_class,
                                  sizeof (CsdSmartcardManagerPrivate));
}

static void
csd_smartcard_manager_class_install_properties (CsdSmartcardManagerClass *card_class)
{
        GObjectClass *object_class;
        GParamSpec *param_spec;

        object_class = G_OBJECT_CLASS (card_class);
        object_class->set_property = csd_smartcard_manager_set_property;
        object_class->get_property = csd_smartcard_manager_get_property;

        param_spec = g_param_spec_string ("module-path", "Module Path",
                                          "path to smartcard PKCS #11 driver",
                                          NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class, PROP_MODULE_PATH, param_spec);
}

static void
csd_smartcard_manager_set_property (GObject       *object,
                                    guint          prop_id,
                                    const GValue  *value,
                                    GParamSpec    *pspec)
{
        CsdSmartcardManager *manager = CSD_SMARTCARD_MANAGER (object);

        switch (prop_id) {
                case PROP_MODULE_PATH:
                        csd_smartcard_manager_set_module_path (manager,
                                                                   g_value_get_string (value));
                        break;

                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
        }
}

static void
csd_smartcard_manager_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        CsdSmartcardManager *manager = CSD_SMARTCARD_MANAGER (object);
        char *module_path;

        switch (prop_id) {
                case PROP_MODULE_PATH:
                        module_path = csd_smartcard_manager_get_module_path (manager);
                        g_value_set_string (value, module_path);
                        g_free (module_path);
                        break;

                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
        }
}

char *
csd_smartcard_manager_get_module_path (CsdSmartcardManager *manager)
{
        return manager->priv->module_path;
}

static void
csd_smartcard_manager_set_module_path (CsdSmartcardManager *manager,
                                       const char          *module_path)
{
        if ((manager->priv->module_path == NULL) && (module_path == NULL)) {
                return;
        }

        if (((manager->priv->module_path == NULL) ||
         (module_path == NULL) ||
         (strcmp (manager->priv->module_path, module_path) != 0))) {
                g_free (manager->priv->module_path);
                manager->priv->module_path = g_strdup (module_path);
                g_object_notify (G_OBJECT (manager), "module-path");
        }
}

static void
csd_smartcard_manager_card_removed_handler (CsdSmartcardManager *manager,
                                            CsdSmartcard        *card)
{
        g_debug ("informing smartcard of its removal");
        _csd_smartcard_set_state (card, CSD_SMARTCARD_STATE_REMOVED);
        g_debug ("done");
}

static void
csd_smartcard_manager_card_inserted_handler (CsdSmartcardManager *manager,
                                             CsdSmartcard        *card)
{
        g_debug ("informing smartcard of its insertion");

        _csd_smartcard_set_state (card, CSD_SMARTCARD_STATE_INSERTED);
        g_debug ("done");

}

static void
csd_smartcard_manager_class_install_signals (CsdSmartcardManagerClass *manager_class)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (manager_class);

        csd_smartcard_manager_signals[SMARTCARD_INSERTED] =
                g_signal_new ("smartcard-inserted",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (CsdSmartcardManagerClass,
                                               smartcard_inserted),
                              NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        manager_class->smartcard_inserted = csd_smartcard_manager_card_inserted_handler;

        csd_smartcard_manager_signals[SMARTCARD_REMOVED] =
                g_signal_new ("smartcard-removed",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (CsdSmartcardManagerClass,
                                               smartcard_removed),
                              NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        manager_class->smartcard_removed = csd_smartcard_manager_card_removed_handler;

        csd_smartcard_manager_signals[ERROR] =
                g_signal_new ("error",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (CsdSmartcardManagerClass, error),
                              NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
        manager_class->error = NULL;
}

static gboolean
slot_id_equal (CK_SLOT_ID *slot_id_1,
               CK_SLOT_ID *slot_id_2)
{
        g_assert (slot_id_1 != NULL);
        g_assert (slot_id_2 != NULL);

        return *slot_id_1 == *slot_id_2;
}

static gboolean
slot_id_hash (CK_SLOT_ID *slot_id)
{
        guint32 upper_bits, lower_bits;
        int temp;

        if (sizeof (CK_SLOT_ID) == sizeof (int)) {
                return g_int_hash (slot_id);
        }

        upper_bits = ((*slot_id) >> 31) - 1;
        lower_bits = (*slot_id) & 0xffffffff;

        /* The upper bits are almost certainly always zero,
         * so let's degenerate to g_int_hash for the
         * (very) common case
         */
        temp = lower_bits + upper_bits;
        return upper_bits + g_int_hash (&temp);
}

static void
csd_smartcard_manager_init (CsdSmartcardManager *manager)
{
        g_debug ("initializing smartcard manager");

        manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                                     CSD_TYPE_SMARTCARD_MANAGER,
                                                     CsdSmartcardManagerPrivate);
        manager->priv->poll_timeout_id = 0;
        manager->priv->is_unstoppable = FALSE;

        manager->priv->smartcards =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GDestroyNotify) g_free,
                                       (GDestroyNotify) g_object_unref);
}

static void
csd_smartcard_manager_finalize (GObject *object)
{
        CsdSmartcardManager *manager;
        GObjectClass *gobject_class;

        manager = CSD_SMARTCARD_MANAGER (object);
        gobject_class =
                G_OBJECT_CLASS (csd_smartcard_manager_parent_class);

        csd_smartcard_manager_stop_now (manager);

        g_hash_table_destroy (manager->priv->smartcards);
        manager->priv->smartcards = NULL;

        gobject_class->finalize (object);
}

GQuark
csd_smartcard_manager_error_quark (void)
{
        static GQuark error_quark = 0;

        if (error_quark == 0) {
                error_quark = g_quark_from_static_string ("csd-smartcard-manager-error-quark");
        }

        return error_quark;
}

CsdSmartcardManager *
csd_smartcard_manager_new_default (void)
{
  return csd_smartcard_manager_new (NULL);
}

CsdSmartcardManager *
csd_smartcard_manager_new (const char *module_path)
{
        CsdSmartcardManager *instance;

        instance = CSD_SMARTCARD_MANAGER (g_object_new (CSD_TYPE_SMARTCARD_MANAGER,
                                                        "module-path", module_path,
                                                        NULL));

        return instance;
}

static void
csd_smartcard_manager_emit_error (CsdSmartcardManager *manager,
                                  GError              *error)
{
        manager->priv->is_unstoppable = TRUE;
        g_signal_emit (manager, csd_smartcard_manager_signals[ERROR], 0,
                       error);
        manager->priv->is_unstoppable = FALSE;
}

static void
csd_smartcard_manager_emit_smartcard_inserted (CsdSmartcardManager *manager,
                                               CsdSmartcard        *card)
{
        manager->priv->is_unstoppable = TRUE;
        g_signal_emit (manager, csd_smartcard_manager_signals[SMARTCARD_INSERTED], 0,
                       card);
        manager->priv->is_unstoppable = FALSE;
}

static void
csd_smartcard_manager_emit_smartcard_removed (CsdSmartcardManager *manager,
                                              CsdSmartcard        *card)
{
        manager->priv->is_unstoppable = TRUE;
        g_signal_emit (manager, csd_smartcard_manager_signals[SMARTCARD_REMOVED], 0,
                       card);
        manager->priv->is_unstoppable = FALSE;
}

static gboolean
csd_smartcard_manager_check_for_and_process_events (GIOChannel          *io_channel,
                                                    GIOCondition         condition,
                                                    CsdSmartcardManagerWorker *worker)
{
        CsdSmartcard *card;
        CsdSmartcardManager *manager;
        gboolean should_stop;
        guchar event_type;
        char *card_name;
        int fd;

        manager = worker->manager;

        g_debug ("event!");
        card = NULL;
        should_stop = (condition & G_IO_HUP) || (condition & G_IO_ERR);

        if (should_stop) {
                g_debug ("received %s on event socket, stopping "
                          "manager...",
                          (condition & G_IO_HUP) && (condition & G_IO_ERR)?
                          "error and hangup" :
                          (condition & G_IO_HUP)?
                          "hangup" : "error");
        }

        if (!(condition & G_IO_IN)) {
                g_debug ("nevermind outta here!");
                goto out;
        }

        fd = g_io_channel_unix_get_fd (io_channel);

        event_type = '\0';
        if (!read_bytes (fd, &event_type, 1)) {
                g_debug ("could not read event type, stopping");
                should_stop = TRUE;
                goto out;
        }

        card = read_smartcard (fd, worker->module);

        if (card == NULL) {
                g_debug ("could not read card, stopping");
                should_stop = TRUE;
                goto out;
        }

        card_name = csd_smartcard_get_name (card);
        g_debug ("card '%s' had event %c", card_name, event_type);

        switch (event_type) {
                case 'I':
                        g_hash_table_replace (manager->priv->smartcards,
                                              card_name, card);
                        card_name = NULL;

                        csd_smartcard_manager_emit_smartcard_inserted (manager, card);
                        card = NULL;
                        break;

                case 'R':
                        csd_smartcard_manager_emit_smartcard_removed (manager, card);
                        if (!g_hash_table_remove (manager->priv->smartcards, card_name)) {
                                g_debug ("got removal event of unknown card!");
                        }
                        g_free (card_name);
                        card_name = NULL;
                        card = NULL;
                        break;

                default:
                        g_free (card_name);
                        card_name = NULL;
                        g_object_unref (card);

                        should_stop = TRUE;
                        break;
        }

out:
        if (should_stop) {
                GError *error;

                error = g_error_new (CSD_SMARTCARD_MANAGER_ERROR,
                                     CSD_SMARTCARD_MANAGER_ERROR_WATCHING_FOR_EVENTS,
                                     "%s", (condition & G_IO_IN) ? g_strerror (errno) : _("received error or hang up from event source"));

                csd_smartcard_manager_emit_error (manager, error);
                g_error_free (error);
                csd_smartcard_manager_stop_now (manager);
                return FALSE;
        }

        return TRUE;
}

static void
stop_manager (CsdSmartcardManager *manager)
{
        manager->priv->state = CSD_SMARTCARD_MANAGER_STATE_STOPPED;

        if (manager->priv->nss_is_loaded) {
                NSS_Shutdown ();
                manager->priv->nss_is_loaded = FALSE;
        }
        g_debug ("smartcard manager stopped");
}

static void
stop_worker (CsdSmartcardManagerWorker *worker)
{
        CsdSmartcardManager *manager;

        manager = worker->manager;

        if (worker->event_source != NULL) {
                g_source_destroy (worker->event_source);
                worker->event_source = NULL;
        }

        if (worker->thread != NULL) {
                SECMOD_CancelWait (worker->module);
                worker->thread = NULL;
        }

        SECMOD_DestroyModule (worker->module);
        manager->priv->workers = g_list_remove (manager->priv->workers, worker);

        if (manager->priv->workers == NULL && manager->priv->state != CSD_SMARTCARD_MANAGER_STATE_STOPPED) {
                stop_manager (manager);
        }
}

static void
csd_smartcard_manager_event_processing_stopped_handler (CsdSmartcardManagerWorker *worker)
{
        worker->event_source = NULL;

        stop_worker (worker);
}

static gboolean
open_pipe (int *write_fd,
           int *read_fd)
{
        int pipe_fds[2] = { -1, -1 };

        g_assert (write_fd != NULL);
        g_assert (read_fd != NULL);

        if (pipe (pipe_fds) < 0) {
                return FALSE;
        }

        if (fcntl (pipe_fds[0], F_SETFD, FD_CLOEXEC) < 0) {
                close (pipe_fds[0]);
                close (pipe_fds[1]);
                return FALSE;
        }

        if (fcntl (pipe_fds[1], F_SETFD, FD_CLOEXEC) < 0) {
                close (pipe_fds[0]);
                close (pipe_fds[1]);
                return FALSE;
        }

        *read_fd = pipe_fds[0];
        *write_fd = pipe_fds[1];

        return TRUE;
}

static void
csd_smartcard_manager_stop_watching_for_events (CsdSmartcardManager  *manager)
{
        GList *node;

        node = manager->priv->workers;
        while (node != NULL) {
                CsdSmartcardManagerWorker *worker;
                GList *next_node;

                worker = (CsdSmartcardManagerWorker *) node->data;
                next_node = node->next;

                stop_worker (worker);

                node = next_node;
        }
}

static gboolean
load_nss (GError **error)
{
        SECStatus status = SECSuccess;
        static const guint32 flags =
        NSS_INIT_READONLY |
        NSS_INIT_FORCEOPEN | NSS_INIT_NOROOTINIT |
        NSS_INIT_OPTIMIZESPACE | NSS_INIT_PK11RELOAD;

        g_debug ("attempting to load NSS database '%s'",
                 CSD_SMARTCARD_MANAGER_NSS_DB);

        PR_Init (PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

        status = NSS_Initialize (CSD_SMARTCARD_MANAGER_NSS_DB,
                                 "", "", SECMOD_DB, flags);

        if (status != SECSuccess) {
                gsize error_message_size;
                char *error_message;

                error_message_size = PR_GetErrorTextLength ();

                if (error_message_size == 0) {
                        g_debug ("NSS security system could not be initialized");
                        g_set_error (error,
                                     CSD_SMARTCARD_MANAGER_ERROR,
                                     CSD_SMARTCARD_MANAGER_ERROR_WITH_NSS,
                                     _("NSS security system could not be initialized"));
                        goto out;
                }

                error_message = g_slice_alloc0 (error_message_size);
                PR_GetErrorText (error_message);

                g_set_error (error,
                             CSD_SMARTCARD_MANAGER_ERROR,
                             CSD_SMARTCARD_MANAGER_ERROR_WITH_NSS,
                             "%s", error_message);
                g_debug ("NSS security system could not be initialized - %s",
                          error_message);

                g_slice_free1 (error_message_size, error_message);

                goto out;
        }

        g_debug ("NSS database successfully loaded");
        return TRUE;

out:
        g_debug ("NSS database couldn't be successfully loaded");
        return FALSE;
}

static GList *
get_available_modules (CsdSmartcardManager  *manager)
{
        SECMODModuleList *module_list, *tmp;
        GList *modules;

        g_debug ("Getting list of suitable modules");

        module_list = SECMOD_GetDefaultModuleList ();
        modules = NULL;
        for (tmp = module_list; tmp != NULL; tmp = tmp->next) {
                if (!SECMOD_HasRemovableSlots (tmp->module) ||
                    !tmp->module->loaded)
                        continue;

                g_debug ("Using module '%s'", tmp->module->commonName);

                modules = g_list_prepend (modules,
                                          SECMOD_ReferenceModule (tmp->module));
        }

        return modules;
}

static gboolean
load_driver (CsdSmartcardManager  *manager,
             char                 *module_path,
             GError              **error)
{
        GList *modules;
        char *module_spec;
        gboolean module_explicitly_specified;

        g_debug ("attempting to load driver...");

        modules = NULL;
        module_explicitly_specified = module_path != NULL;
        if (module_explicitly_specified) {
                SECMODModule *module;

                module_spec = g_strdup_printf ("library=\"%s\"", module_path);
                g_debug ("loading smartcard driver using spec '%s'",
                          module_spec);

                module = SECMOD_LoadUserModule (module_spec,
                                                NULL /* parent */,
                                                FALSE /* recurse */);
                g_free (module_spec);
                module_spec = NULL;

                if (SECMOD_HasRemovableSlots (module) &&
                    module->loaded) {
                        modules = g_list_prepend (modules, module);
                } else {
                        g_debug ("fallback module found but not %s",
                                 SECMOD_HasRemovableSlots (module)?
                                 "removable" : "loaded");
                        SECMOD_DestroyModule (module);
                }

        } else {
                SECMODListLock *lock;

                lock = SECMOD_GetDefaultModuleListLock ();

                if (lock != NULL) {
                        SECMOD_GetReadLock (lock);
                        modules = get_available_modules (manager);
                        SECMOD_ReleaseReadLock (lock);
                }

                /* fallback to compiled in driver path
                 */
                if (modules == NULL) {
                        SECMODModule *module;
                        module_path = CSD_SMARTCARD_MANAGER_DRIVER;
                        module_spec = g_strdup_printf ("library=\"%s\"", module_path);
                        g_debug ("loading smartcard driver using spec '%s'",
                                module_spec);

                        module = SECMOD_LoadUserModule (module_spec,
                                NULL /* parent */,
                                FALSE /* recurse */);
                        g_free (module_spec);
                        module_spec = NULL;

                        if (SECMOD_HasRemovableSlots (module) &&
                            module->loaded) {
                                modules = g_list_prepend (modules, module);
                        } else {
                                g_debug ("fallback module found but not loaded");
                                SECMOD_DestroyModule (module);
                        }
                }

        }

        if (!module_explicitly_specified && modules == NULL) {
                g_set_error (error,
                             CSD_SMARTCARD_MANAGER_ERROR,
                             CSD_SMARTCARD_MANAGER_ERROR_LOADING_DRIVER,
                             _("no suitable smartcard driver could be found"));
        } else if (modules == NULL) {

                gsize error_message_size;
                char *error_message;

                error_message_size = PR_GetErrorTextLength ();

                if (error_message_size == 0) {
                        g_debug ("smartcard driver '%s' could not be loaded",
                                  module_path);
                        g_set_error (error,
                                     CSD_SMARTCARD_MANAGER_ERROR,
                                     CSD_SMARTCARD_MANAGER_ERROR_LOADING_DRIVER,
                                     _("smartcard driver '%s' could not be "
                                       "loaded"), module_path);
                        goto out;
                }

                error_message = g_slice_alloc0 (error_message_size);
                PR_GetErrorText (error_message);

                g_set_error (error,
                             CSD_SMARTCARD_MANAGER_ERROR,
                             CSD_SMARTCARD_MANAGER_ERROR_LOADING_DRIVER,
                             "%s", error_message);

                g_debug ("smartcard driver '%s' could not be loaded - %s",
                          module_path, error_message);
                g_slice_free1 (error_message_size, error_message);
        }

        manager->priv->modules = modules;
out:
        return manager->priv->modules != NULL;
}

static void
csd_smartcard_manager_get_all_cards (CsdSmartcardManager *manager)
{
        GList *node;
        int i;

        node = manager->priv->workers;
        while (node != NULL) {

                CsdSmartcardManagerWorker *worker;

                worker = (CsdSmartcardManagerWorker *) node->data;

                for (i = 0; i < worker->module->slotCount; i++) {
                        CsdSmartcard *card;
                        CK_SLOT_ID    slot_id;
                        int          slot_series;
                        char         *card_name;

                        slot_id = PK11_GetSlotID (worker->module->slots[i]);
                        slot_series = PK11_GetSlotSeries (worker->module->slots[i]);

                        card = _csd_smartcard_new (worker->module,
                                                   slot_id, slot_series);

                        card_name = csd_smartcard_get_name (card);

                        g_hash_table_replace (manager->priv->smartcards,
                                              card_name, card);
                }
                node = node->next;
        }
}

static CsdSmartcardManagerWorker *
start_worker (CsdSmartcardManager  *manager,
              SECMODModule         *module,
              GError              **error)
{
        GIOChannel *io_channel;
        GSource *source;
        CsdSmartcardManagerWorker *worker;

        worker = csd_smartcard_manager_create_worker (manager, module);

        if (worker == NULL) {
                g_set_error (error,
                             CSD_SMARTCARD_MANAGER_ERROR,
                             CSD_SMARTCARD_MANAGER_ERROR_WATCHING_FOR_EVENTS,
                             _("could not watch for incoming card events - %s"),
                             g_strerror (errno));

                goto out;
        }

        io_channel = g_io_channel_unix_new (worker->manager_fd);

        source = g_io_create_watch (io_channel, G_IO_IN | G_IO_HUP);
        g_io_channel_unref (io_channel);
        io_channel = NULL;

        worker->event_source = source;

        g_source_set_callback (worker->event_source,
                               (GSourceFunc) (GIOFunc)
                               csd_smartcard_manager_check_for_and_process_events,
                               worker,
                               (GDestroyNotify)
                               csd_smartcard_manager_event_processing_stopped_handler);
        g_source_attach (worker->event_source, NULL);
        g_source_unref (worker->event_source);
out:
        return worker;
}

static void
start_workers (CsdSmartcardManager *manager)
{
        GList        *node;

        node = manager->priv->modules;
        while (node != NULL) {
                SECMODModule *module;
                CsdSmartcardManagerWorker *worker;
                GError *error;

                module = (SECMODModule *) node->data;

                error = NULL;
                worker = start_worker (manager, module, &error);
                if (worker == NULL) {
                        g_warning ("%s", error->message);
                        g_error_free (error);
                } else {
                        manager->priv->workers = g_list_prepend (manager->priv->workers,
                                                                 worker);
                }
                node = node->next;
        }
}

gboolean
csd_smartcard_manager_start (CsdSmartcardManager  *manager,
                             GError              **error)
{
        GError *nss_error;

        if (manager->priv->state == CSD_SMARTCARD_MANAGER_STATE_STARTED) {
                g_debug ("smartcard manager already started");
                return TRUE;
        }

        manager->priv->state = CSD_SMARTCARD_MANAGER_STATE_STARTING;

        nss_error = NULL;
        if (!manager->priv->nss_is_loaded && !load_nss (&nss_error)) {
                g_propagate_error (error, nss_error);
                goto out;
        }
        manager->priv->nss_is_loaded = TRUE;

        if (manager->priv->modules == NULL) {
                if (!load_driver (manager, manager->priv->module_path, &nss_error)) {
                        g_propagate_error (error, nss_error);
                        goto out;
                }
        }

        start_workers (manager);

        /* populate the hash with cards that are already inserted
         */
        csd_smartcard_manager_get_all_cards (manager);

        manager->priv->state = CSD_SMARTCARD_MANAGER_STATE_STARTED;

out:
        /* don't leave it in a half started state
         */
        if (manager->priv->state != CSD_SMARTCARD_MANAGER_STATE_STARTED) {
                g_debug ("smartcard manager could not be completely started");
                csd_smartcard_manager_stop (manager);
        } else {
                g_debug ("smartcard manager started");
        }

        return manager->priv->state == CSD_SMARTCARD_MANAGER_STATE_STARTED;
}

static gboolean
csd_smartcard_manager_stop_now (CsdSmartcardManager *manager)
{
        if (manager->priv->state == CSD_SMARTCARD_MANAGER_STATE_STOPPED) {
                return FALSE;
        }

        csd_smartcard_manager_stop_watching_for_events (manager);

        return FALSE;
}

static void
csd_smartcard_manager_queue_stop (CsdSmartcardManager *manager)
{

        manager->priv->state = CSD_SMARTCARD_MANAGER_STATE_STOPPING;

        g_idle_add ((GSourceFunc) csd_smartcard_manager_stop_now, manager);
}

void
csd_smartcard_manager_stop (CsdSmartcardManager *manager)
{
        if (manager->priv->state == CSD_SMARTCARD_MANAGER_STATE_STOPPED) {
                return;
        }

        if (manager->priv->is_unstoppable) {
                csd_smartcard_manager_queue_stop (manager);
                return;
        }

        csd_smartcard_manager_stop_now (manager);
}

static void
csd_smartcard_manager_check_for_login_card (CK_SLOT_ID    slot_id,
                                            CsdSmartcard *card,
                                            gboolean     *is_inserted)
{
        g_assert (is_inserted != NULL);

        if (csd_smartcard_is_login_card (card)) {
                *is_inserted = TRUE;
        }

}

gboolean
csd_smartcard_manager_login_card_is_inserted (CsdSmartcardManager *manager)

{
        gboolean is_inserted;

        is_inserted = FALSE;
        g_hash_table_foreach (manager->priv->smartcards,
                              (GHFunc)
                              csd_smartcard_manager_check_for_login_card,
                              &is_inserted);
        return is_inserted;
}

static CsdSmartcardManagerWorker *
csd_smartcard_manager_worker_new (CsdSmartcardManager *manager,
                                  int                 worker_fd,
                                  int                 manager_fd,
                                  SECMODModule        *module)
{
        CsdSmartcardManagerWorker *worker;

        worker = g_slice_new0 (CsdSmartcardManagerWorker);
        worker->manager = manager;
        worker->fd = worker_fd;
        worker->manager_fd = manager_fd;
        worker->module = module;

        worker->smartcards =
                g_hash_table_new_full ((GHashFunc) slot_id_hash,
                                       (GEqualFunc) slot_id_equal,
                                       (GDestroyNotify) g_free,
                                       (GDestroyNotify) g_object_unref);

        return worker;
}

static void
csd_smartcard_manager_worker_free (CsdSmartcardManagerWorker *worker)
{
        if (worker->smartcards != NULL) {
                g_hash_table_destroy (worker->smartcards);
                worker->smartcards = NULL;
        }

        g_slice_free (CsdSmartcardManagerWorker, worker);
}

static gboolean
read_bytes (int      fd,
            gpointer bytes,
            gsize    num_bytes)
{
        size_t bytes_left;
        size_t total_bytes_read;
        ssize_t bytes_read;

        bytes_left = (size_t) num_bytes;
        total_bytes_read = 0;

        do {
                bytes_read = read (fd,
                                   (char *) bytes + total_bytes_read,
                                   bytes_left);
                g_assert (bytes_read <= (ssize_t) bytes_left);

                if (bytes_read <= 0) {
                        if ((bytes_read < 0) && (errno == EINTR || errno == EAGAIN)) {
                                continue;
                        }

                        bytes_left = 0;
                } else {
                        bytes_left -= bytes_read;
                        total_bytes_read += bytes_read;
                }
        } while (bytes_left > 0);

        if (total_bytes_read <  (size_t) num_bytes) {
                return FALSE;
        }

        return TRUE;
}

static gboolean
write_bytes (int           fd,
             gconstpointer bytes,
             gsize         num_bytes)
{
        size_t bytes_left;
        size_t total_bytes_written;
        ssize_t bytes_written;

        bytes_left = (size_t) num_bytes;
        total_bytes_written = 0;

        do {
                bytes_written = write (fd,
                                       (char *) bytes + total_bytes_written,
                                       bytes_left);
                g_assert (bytes_written <= (ssize_t) bytes_left);

                if (bytes_written <= 0) {
                        if ((bytes_written < 0) && (errno == EINTR || errno == EAGAIN)) {
                                continue;
                        }

                        bytes_left = 0;
                } else {
                        bytes_left -= bytes_written;
                        total_bytes_written += bytes_written;
                }
        } while (bytes_left > 0);

        if (total_bytes_written <  (size_t) num_bytes) {
                return FALSE;
        }

        return TRUE;
}

static CsdSmartcard *
read_smartcard (int           fd,
                SECMODModule *module)
{
        CsdSmartcard *card;
        char *card_name;
        gsize card_name_size;

        card_name_size = 0;
        if (!read_bytes (fd, &card_name_size, sizeof (card_name_size))) {
                return NULL;
        }

        card_name = g_slice_alloc0 (card_name_size);
        if (!read_bytes (fd, card_name, card_name_size)) {
                g_slice_free1 (card_name_size, card_name);
                return NULL;
        }
        card = _csd_smartcard_new_from_name (module, card_name);
        g_slice_free1 (card_name_size, card_name);

        return card;
}

static gboolean
write_smartcard (int          fd,
                    CsdSmartcard *card)
{
        gsize card_name_size;
        char *card_name;

        card_name = csd_smartcard_get_name (card);
        card_name_size = strlen (card_name) + 1;

        if (!write_bytes (fd, &card_name_size, sizeof (card_name_size))) {
                g_free (card_name);
                return FALSE;
        }

        if (!write_bytes (fd, card_name, card_name_size)) {
                g_free (card_name);
                return FALSE;
        }
        g_free (card_name);

        return TRUE;
}

static gboolean
csd_smartcard_manager_worker_emit_smartcard_removed (CsdSmartcardManagerWorker  *worker,
                                                     CsdSmartcard               *card,
                                                     GError                    **error)
{
        g_debug ("card '%s' removed!", csd_smartcard_get_name (card));

        if (!write_bytes (worker->fd, "R", 1)) {
                goto error_out;
        }

        if (!write_smartcard (worker->fd, card)) {
                goto error_out;
        }

        return TRUE;

error_out:
        g_set_error (error, CSD_SMARTCARD_MANAGER_ERROR,
                     CSD_SMARTCARD_MANAGER_ERROR_REPORTING_EVENTS,
                     "%s", g_strerror (errno));
        return FALSE;
}

static gboolean
csd_smartcard_manager_worker_emit_smartcard_inserted (CsdSmartcardManagerWorker  *worker,
                                                      CsdSmartcard               *card,
                                                      GError                    **error)
{
        g_debug ("card '%s' inserted!", csd_smartcard_get_name (card));
        if (!write_bytes (worker->fd, "I", 1)) {
                goto error_out;
        }

        if (!write_smartcard (worker->fd, card)) {
                goto error_out;
        }

        return TRUE;

error_out:
        g_set_error (error, CSD_SMARTCARD_MANAGER_ERROR,
                     CSD_SMARTCARD_MANAGER_ERROR_REPORTING_EVENTS,
                     "%s", g_strerror (errno));
        return FALSE;
}

static gboolean
csd_smartcard_manager_worker_watch_for_and_process_event (CsdSmartcardManagerWorker  *worker,
                                                          GError                    **error)
{
        PK11SlotInfo *slot;
        CK_SLOT_ID slot_id, *key = NULL;
        int slot_series, card_slot_series;
        CsdSmartcard *card;
        GError *processing_error;
        gboolean ret;

        g_debug ("waiting for card event");
        ret = FALSE;

        slot = SECMOD_WaitForAnyTokenEvent (worker->module, 0, PR_SecondsToInterval (1));

        processing_error = NULL;

        if (slot == NULL) {
                int error_code;

                error_code = PORT_GetError ();
                if ((error_code == 0) || (error_code == SEC_ERROR_NO_EVENT)) {
                        g_debug ("spurious event occurred");
                        return TRUE;
                }

                /* FIXME: is there a function to convert from a PORT error
                 * code to a translated string?
                 */
                g_set_error (error, CSD_SMARTCARD_MANAGER_ERROR,
                             CSD_SMARTCARD_MANAGER_ERROR_WITH_NSS,
                             _("encountered unexpected error while "
                               "waiting for smartcard events"));
                goto out;
        }

        /* the slot id and series together uniquely identify a card.
         * You can never have two cards with the same slot id at the
         * same time, however (I think), so we can key off of it.
         */
        slot_id = PK11_GetSlotID (slot);
        slot_series = PK11_GetSlotSeries (slot);

        /* First check to see if there is a card that we're currently
         * tracking in the slot.
         */
        key = g_new (CK_SLOT_ID, 1);
        *key = slot_id;
        card = g_hash_table_lookup (worker->smartcards, key);

        if (card != NULL) {
                card_slot_series = csd_smartcard_get_slot_series (card);
        } else {
                card_slot_series = -1;
        }

        if (PK11_IsPresent (slot)) {
                /* Now, check to see if their is a new card in the slot.
                 * If there was a different card in the slot now than
                 * there was before, then we need to emit a removed signal
                 * for the old card (we don't want unpaired insertion events).
                 */
                if ((card != NULL) &&
                    card_slot_series != slot_series) {
                        if (!csd_smartcard_manager_worker_emit_smartcard_removed (worker, card, &processing_error)) {
                                g_propagate_error (error, processing_error);
                                goto out;
                        }
                }

                card = _csd_smartcard_new (worker->module,
                                           slot_id, slot_series);

                g_hash_table_replace (worker->smartcards,
                                      key, card);
                key = NULL;

                if (!csd_smartcard_manager_worker_emit_smartcard_inserted (worker, card, &processing_error)) {
                        g_propagate_error (error, processing_error);
                        goto out;
                }
        } else {
                /* if we aren't tracking the card, just discard the event.
                 * We don't want unpaired remove events.  Note on startup
                 * NSS will generate an "insertion" event if a card is
                 * already inserted in the slot.
                 */
                if ((card != NULL)) {
                        /* FIXME: i'm not sure about this code.  Maybe we
                         * shouldn't do this at all, or maybe we should do it
                         * n times (where n = slot_series - card_slot_series + 1)
                         *
                         * Right now, i'm just doing it once.
                         */
                        if ((slot_series - card_slot_series) > 1) {

                                if (!csd_smartcard_manager_worker_emit_smartcard_removed (worker, card, &processing_error)) {
                                        g_propagate_error (error, processing_error);
                                        goto out;
                                }
                                g_hash_table_remove (worker->smartcards, key);

                                card = _csd_smartcard_new (worker->module,
                                                                slot_id, slot_series);
                                g_hash_table_replace (worker->smartcards,
                                                      key, card);
                                key = NULL;
                                if (!csd_smartcard_manager_worker_emit_smartcard_inserted (worker, card, &processing_error)) {
                                        g_propagate_error (error, processing_error);
                                        goto out;
                                }
                        }

                        if (!csd_smartcard_manager_worker_emit_smartcard_removed (worker, card, &processing_error)) {
                                g_propagate_error (error, processing_error);
                                goto out;
                        }

                        g_hash_table_remove (worker->smartcards, key);
                        card = NULL;
                } else {
                        g_debug ("got spurious remove event");
                }
        }

        ret = TRUE;

out:
        g_free (key);
        PK11_FreeSlot (slot);

        return ret;
}

static void
csd_smartcard_manager_worker_run (CsdSmartcardManagerWorker *worker)
{
        GError *error;
        gboolean should_continue;

        do
        {
                error = NULL;
                should_continue = csd_smartcard_manager_worker_watch_for_and_process_event (worker, &error);
        }
        while (should_continue);

        if (error != NULL)  {
                g_debug ("could not process card event - %s", error->message);
                g_error_free (error);
        }

        csd_smartcard_manager_worker_free (worker);
}

static CsdSmartcardManagerWorker *
csd_smartcard_manager_create_worker (CsdSmartcardManager  *manager,
                                     SECMODModule         *module)
{
        CsdSmartcardManagerWorker *worker;
        int write_fd, read_fd;

        write_fd = -1;
        read_fd = -1;
        if (!open_pipe (&write_fd, &read_fd)) {
                return NULL;
        }

        worker = csd_smartcard_manager_worker_new (manager,
                                                   write_fd,
                                                   read_fd,
                                                   module);

        worker->thread = g_thread_create ((GThreadFunc)
                                          csd_smartcard_manager_worker_run,
                                          worker, FALSE, NULL);

        if (worker->thread == NULL) {
                csd_smartcard_manager_worker_free (worker);
                return NULL;
        }

        return worker;
}

#ifdef CSD_SMARTCARD_MANAGER_ENABLE_TEST
#include <glib.h>

static GMainLoop *event_loop;
static gboolean should_exit_on_next_remove = FALSE;

static gboolean
on_timeout (CsdSmartcardManager *manager)
{
        GError *error;
        g_print ("Re-enabling manager.\n");

        if (!csd_smartcard_manager_start (manager, &error)) {
                g_warning ("could not start smartcard manager - %s",
                           error->message);
                g_error_free (error);
                return TRUE;
        }
        g_print ("Please re-insert smartcard\n");

        should_exit_on_next_remove = TRUE;

        return FALSE;
}

static void
on_device_inserted (CsdSmartcardManager *manager,
                    CsdSmartcard        *card)
{
        g_print ("smartcard inserted!\n");
        g_print ("Please remove it.\n");
}

static void
on_device_removed (CsdSmartcardManager *manager,
                   CsdSmartcard        *card)
{
        g_print ("smartcard removed!\n");

        if (should_exit_on_next_remove) {
                g_main_loop_quit (event_loop);
        } else {
                g_print ("disabling manager for 2 seconds\n");
                csd_smartcard_manager_stop (manager);
                g_timeout_add_seconds (2, (GSourceFunc) on_timeout, manager);
        }
}

int
main (int   argc,
      char *argv[])
{
        CsdSmartcardManager *manager;
        GError *error;

        g_log_set_always_fatal (G_LOG_LEVEL_ERROR
                                | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);


        g_message ("creating instance of 'smartcard manager' object...");
        manager = csd_smartcard_manager_new (NULL);
        g_message ("'smartcard manager' object created successfully");

        g_signal_connect (manager, "smartcard-inserted",
                          G_CALLBACK (on_device_inserted), NULL);

        g_signal_connect (manager, "smartcard-removed",
                          G_CALLBACK (on_device_removed), NULL);

        g_message ("starting listener...");

        error = NULL;
        if (!csd_smartcard_manager_start (manager, &error)) {
                g_warning ("could not start smartcard manager - %s",
                           error->message);
                g_error_free (error);
                return 1;
        }

        event_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (event_loop);
        g_main_loop_unref (event_loop);
        event_loop = NULL;

        g_message ("destroying previously created 'smartcard manager' object...");
        g_object_unref (manager);
        manager = NULL;
        g_message ("'smartcard manager' object destroyed successfully");

        return 0;
}
#endif
