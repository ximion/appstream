/* SPDX-License-Identifier: LGPL-2.1-or-later */
@@
expression p;
@@
- g_free (p);
- p = NULL;
+ g_free (g_steal_pointer (&p));
