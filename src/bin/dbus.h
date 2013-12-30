#ifndef _DBUS_H__
#define _DBUS_H__ 1

void ty_dbus_link_hide(void);
void ty_dbus_link_mousein(uint64_t win, const char *url, int x, int y);
void ty_dbus_link_mouseout(uint64_t win, const char *url, int x, int y);
void ty_dbus_init(void);
void ty_dbus_shutdown(void);

#endif
