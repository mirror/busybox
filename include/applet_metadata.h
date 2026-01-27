/* vi: set sw=4 ts=4: */
/*
 * PiPlayInit Applet Metadata
 *
 * Canonical metadata definitions for PiPlayInit-managed applets.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#ifndef PIPLAYINIT_APPLET_METADATA_H
#define PIPLAYINIT_APPLET_METADATA_H 1

/* Note:
 * This header may be included by both host and target builds.
 * It defines canonical metadata used by PiPlayInit during
 * early userspace and init-stage execution.
 */

/*
 * Install locations for PiPlayInit applets.
 *
 * Order matters: values are used as indices into install
 * directory tables during build and install stages.
 *
 * These locations are system-level and authority-owned
 * by PiPlayInit, not userland.
 */
typedef enum ppi_install_loc_t {
	PPI_DIR_ROOT = 0,
	PPI_DIR_BIN,
	PPI_DIR_SBIN,
#if ENABLE_INSTALL_NO_USR
	PPI_DIR_USR_BIN  = PPI_DIR_BIN,
	PPI_DIR_USR_SBIN = PPI_DIR_SBIN,
#else
	PPI_DIR_USR_BIN,
	PPI_DIR_USR_SBIN,
#endif
} ppi_install_loc_t;

/*
 * Privilege handling policy for PiPlayInit applets.
 *
 * Applets declare intent; PiPlayInit enforces authority.
 */
typedef enum ppi_suid_t {
	PPI_SUID_DROP = 0,     /* Drop privileges immediately */
	PPI_SUID_MAYBE,        /* Conditional privilege use */
	PPI_SUID_REQUIRE       /* Requires elevated privilege */
} ppi_suid_t;

#endif /* PIPLAYINIT_APPLET_METADATA_H */