/* vi: set sw=4 ts=4: */
/*
 * applets.src.h — listing of all PiPlayInit applets.
 *
 * If you write a new applet, you must add an entry to this list to make
 * PiPlayInit aware of it.
 *
 * This file is a source template used to generate applet tables,
 * prototypes, usage text, install links, and privilege metadata.
 */

/*
name  - applet name as invoked
help  - applet name converted to C identifier form
main  - corresponding <applet>_main entry point
l     - install location (system-owned)
s     - privilege policy:
        PPI_SUID_REQUIRE: requires elevated privilege
        PPI_SUID_DROP:    drops privileges before execution
        PPI_SUID_MAYBE:   conditional privilege usage

        All uses of PPI_SUID_REQUIRE and PPI_SUID_MAYBE
        must be explicitly justified.

        NOTE: update FEATURE_SUID help text when changing
        any PPI_SUID_* usage.
*/

#if defined(PROTOTYPES)
# define APPLET(name,l,s)                    int name##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
# define APPLET_ODDNAME(name,main,l,s,help)  int main##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
# define APPLET_NOEXEC(name,main,l,s,help)   int main##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
# define APPLET_NOFORK(name,main,l,s,help)   int main##_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
# define APPLET_SCRIPTED(name,main,l,s,help)

#elif defined(NAME_MAIN)
# define APPLET(name,l,s)                    name name##_main
# define APPLET_ODDNAME(name,main,l,s,help)  name main##_main
# define APPLET_NOEXEC(name,main,l,s,help)   name main##_main
# define APPLET_NOFORK(name,main,l,s,help)   name main##_main
# define APPLET_SCRIPTED(name,main,l,s,help) name scripted_main

#elif defined(MAKE_USAGE) && ENABLE_FEATURE_VERBOSE_USAGE
# define APPLET(name,l,s)                    MAKE_USAGE(#name, name##_trivial_usage name##_full_usage)
# define APPLET_ODDNAME(name,main,l,s,help)  MAKE_USAGE(#name, help##_trivial_usage help##_full_usage)
# define APPLET_NOEXEC(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage help##_full_usage)
# define APPLET_NOFORK(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage help##_full_usage)
# define APPLET_SCRIPTED(name,main,l,s,help) MAKE_USAGE(#name, help##_trivial_usage help##_full_usage)

#elif defined(MAKE_USAGE) && !ENABLE_FEATURE_VERBOSE_USAGE
# define APPLET(name,l,s)                    MAKE_USAGE(#name, name##_trivial_usage)
# define APPLET_ODDNAME(name,main,l,s,help)  MAKE_USAGE(#name, help##_trivial_usage)
# define APPLET_NOEXEC(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage)
# define APPLET_NOFORK(name,main,l,s,help)   MAKE_USAGE(#name, help##_trivial_usage)
# define APPLET_SCRIPTED(name,main,l,s,help) MAKE_USAGE(#name, help##_trivial_usage)

#elif defined(MAKE_LINKS)
# define APPLET(name,l,s)                    LINK l name
# define APPLET_ODDNAME(name,main,l,s,help)  LINK l name
# define APPLET_NOEXEC(name,main,l,s,help)   LINK l name
# define APPLET_NOFORK(name,main,l,s,help)   LINK l name
# define APPLET_SCRIPTED(name,main,l,s,help) LINK l name

#elif defined(MAKE_SUID)
# define APPLET(name,l,s)                    SUID s l name
# define APPLET_ODDNAME(name,main,l,s,help)  SUID s l name
# define APPLET_NOEXEC(name,main,l,s,help)   SUID s l name
# define APPLET_NOFORK(name,main,l,s,help)   SUID s l name
# define APPLET_SCRIPTED(name,main,l,s,help) SUID s l name

#elif defined(MAKE_SCRIPTS)
# define APPLET(name,l,s)
# define APPLET_ODDNAME(name,main,l,s,help)
# define APPLET_NOEXEC(name,main,l,s,help)
# define APPLET_NOFORK(name,main,l,s,help)
# define APPLET_SCRIPTED(name,main,l,s,help) SCRIPT name

#else
  static struct bb_applet applets[] = { /* name, main, location, privilege */
# define APPLET(name,l,s)                    { #name, #name, l, s },
# define APPLET_ODDNAME(name,main,l,s,help)  { #name, #main, l, s },
# define APPLET_NOEXEC(name,main,l,s,help)   { #name, #main, l, s, 1 },
# define APPLET_NOFORK(name,main,l,s,help)   { #name, #main, l, s, 1, 1 },
# define APPLET_SCRIPTED(name,main,l,s,help) { #name, #main, l, s },
#endif

#if ENABLE_INSTALL_NO_USR
# define PPI_DIR_USR_BIN  PPI_DIR_BIN
# define PPI_DIR_USR_SBIN PPI_DIR_SBIN
#endif


INSERT


#if !defined(PROTOTYPES) && !defined(NAME_MAIN) && !defined(MAKE_USAGE) \
	&& !defined(MAKE_LINKS) && !defined(MAKE_SUID)
};
#endif

#undef APPLET
#undef APPLET_ODDNAME
#undef APPLET_NOEXEC
#undef APPLET_NOFORK
#undef APPLET_SCRIPTED