TODO:

- Remove custom keybinding code (we handle that in Cinnamon now) - do we want to handle media keys in cinnamon also?  Would get around the 'no meda keys while a menu is open' issue.
- Switch to Gnome's keyboard layout (gsettings) handler - basically reverting Ubuntu's patch for this.  This will allow us to implement ibus popups directly in Cinnamon
- Look into backgrounds - we should be able to eliminate the background manager in the cinnamon gnome 3.8 compat rollup, and continue to handle backgrounds as we currently do
- Investigate:  How to keep gnome-settings-daemon from autostarting.  It checks for environment=GNOME... which means Cinnamon also - is it time to have our own freedesktop.org name?
