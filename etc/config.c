/*
 * This is used by the top-level makefile to automagically
 * get the kernel version from <linux/version.h>.
 * A simple grep doesn't work for Mandrake and Red Hat distributions
 * that contain multiple UTS_RELEASE definitions in version.h.
 *
 * Note: since kernel 2.6.17, the definition of UTS_RELEASE has
 * moved to <linux/utsrelease.h>.
 */
#include <linux/version.h>
#ifndef UTS_RELEASE
#include <linux/utsrelease.h>
#endif
char *uts_release=UTS_RELEASE;
