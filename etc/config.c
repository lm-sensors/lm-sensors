/*
 * This is used by the top-level makefile to automagically
 * get the kernel version from <linux/version.h>.
 * A simple grep doesn't work for Mandrake and Red Hat distributions
 * that contain multiple UTS_RELEASE definitions in version.h.
 */
#include <linux/version.h>
char *uts_release=UTS_RELEASE;
