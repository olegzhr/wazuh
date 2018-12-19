/* Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

#include "shared.h"
#include "rootcheck.h"
#include "config/syscheck-config.h"

/* Report a problem */
int notify_rk(int rk_type, const char *msg)
{

    /* No need to alert on that to the server */
    if (rk_type <= ALERT_SYSTEM_ERR) {
        return (0);
    }

    /* Send event/check to the rootcheck queue */
    if (SendMSG(rootcheck.queue, msg, ROOTCHECK, ROOTCHECK_MQ) < 0) {
        mterror(ARGV0, QUEUE_SEND);

        if ((rootcheck.queue = StartMQ(DEFAULTQPATH, WRITE)) < 0) {
            mterror_exit(ARGV0, QUEUE_FATAL, DEFAULTQPATH);
        }

        if (SendMSG(rootcheck.queue, msg, ROOTCHECK, ROOTCHECK_MQ) < 0) {
            mterror_exit(ARGV0, QUEUE_FATAL, DEFAULTQPATH);
        }
    }

    return (0);
}

/* Execute the rootkit checks */
void run_rk_check()
{
    time_t time1;
    time_t time2;
    FILE *fp;
    OSList *plist;

#ifndef WIN32
    /* On non-Windows, always start at / */
    char basedir[] = "";
#else
    /* On Windows, always start at C:\ */
    char basedir[] = "C:\\";
#endif

    /* Set basedir */
    if (rootcheck.basedir == NULL) {
        rootcheck.basedir = strdup(basedir);
    }

    time1 = time(0);

    /* Clean the global variables */
    rk_sys_count = 0;
    rk_sys_file[rk_sys_count] = NULL;
    rk_sys_name[rk_sys_count] = NULL;

    /* Send scan start message */
    notify_rk(ALERT_POLICY_VIOLATION, "Starting rootcheck scan.");
    mtinfo(ARGV0, "Starting rootcheck scan.");

    /* Check for Rootkits */
    /* Open rootkit_files and pass the pointer to check_rc_files */
    if (rootcheck.checks.rc_files) {
        if (!rootcheck.rootkit_files) {
#ifndef WIN32
            mterror(ARGV0, "No rootcheck_files file configured.");
#endif
        } else {
            fp = fopen(rootcheck.rootkit_files, "r");
            if (!fp) {
                mterror(ARGV0, "No rootcheck_files file: '%s'", rootcheck.rootkit_files);
            }

            else {
                check_rc_files(rootcheck.basedir, fp);

                fclose(fp);
            }
        }
    }

    /* Check for trojan entries in common binaries */
    if (rootcheck.checks.rc_trojans) {
        if (!rootcheck.rootkit_trojans) {
#ifndef WIN32
            mterror(ARGV0, "No rootcheck_trojans file configured.");
#endif
        } else {
            fp = fopen(rootcheck.rootkit_trojans, "r");
            if (!fp) {
                mterror(ARGV0, "No rootcheck_trojans file: '%s'", rootcheck.rootkit_trojans);
            } else {
#ifndef HPUX
                check_rc_trojans(rootcheck.basedir, fp);
#endif
                fclose(fp);
            }
        }
    }

#ifdef WIN32
    /* Get process list */
    plist = os_get_process_list();

    /* Windows audit check */
    if (rootcheck.checks.rc_winaudit) {
        if (!rootcheck.winaudit) {
            mtinfo(ARGV0, "No winaudit file configured.");
        } else {
            fp = fopen(rootcheck.winaudit, "r");
            if (!fp) {
                mterror(ARGV0, "No winaudit file: '%s'", rootcheck.winaudit);
            } else {
                if (check_rc_winaudit(fp, plist) < 0) {
                    mterror(ARGV0, "Failed Windows audit for file '%s'", rootcheck.winaudit);
                }
                fclose(fp);
            }
        }
    }

    /* Windows malware */
    if (rootcheck.checks.rc_winmalware) {
        if (!rootcheck.winmalware) {
            mtinfo(ARGV0, "No winmalware file configured.");
        } else {
            fp = fopen(rootcheck.winmalware, "r");
            if (!fp) {
                mterror(ARGV0, "No winmalware file: '%s'", rootcheck.winmalware);
            } else {
                if (check_rc_winmalware(fp, plist) < 0) {
                    mterror(ARGV0, "Failed Windows malware scan for file '%s", rootcheck.winmalware);
                }
                fclose(fp);
            }
        }
    }

    /* Windows Apps */
    if (rootcheck.checks.rc_winapps) {
        if (!rootcheck.winapps) {
            mtinfo(ARGV0, "No winapps file configured.");
        } else {
            fp = fopen(rootcheck.winapps, "r");
            if (!fp) {
                mterror(ARGV0, "No winapps file: '%s'", rootcheck.winapps);
            } else {
                if (check_rc_winapps(fp, plist) < 0) {
                    mterror(ARGV0, "Failed Windows applications scan for file '%s", rootcheck.winapps);
                }
                fclose(fp);
            }
        }
    }

    /* Free the process list */
    del_plist((void *)plist);

#else
    size_t i;
    /* Checks for other non-Windows */

    /* Unix audit check */
    if (rootcheck.checks.rc_unixaudit) {
        if (rootcheck.unixaudit) {
            /* Get process list */
            plist = os_get_process_list();

            i = 0;
            while (rootcheck.unixaudit[i]) {
                fp = fopen(rootcheck.unixaudit[i], "r");
                if (!fp) {
                    mterror(ARGV0, "No unixaudit file: '%s'", rootcheck.unixaudit[i]);
                } else {
                    /* Run unix audit */
                    if (check_rc_unixaudit(fp, plist) < 0) {
                        mterror(ARGV0, "Failed system audit for file '%s'", rootcheck.unixaudit[i]);
                    }
                    fclose(fp);
                }

                i++;
            }

            /* Free list */
            del_plist(plist);
        }
    }

#endif /* !WIN32 */

    /* Check for files in the /dev filesystem */
    if (rootcheck.checks.rc_dev) {
        mtdebug1(ARGV0, "Going into check_rc_dev");
        check_rc_dev(rootcheck.basedir);
    }

    /* Scan the whole system for additional issues */
    if (rootcheck.checks.rc_sys) {
        mtdebug1(ARGV0, "Going into check_rc_sys");
        check_rc_sys(rootcheck.basedir);
    }

    /* Check processes */
    if (rootcheck.checks.rc_pids) {
        mtdebug1(ARGV0, "Going into check_rc_pids");
        check_rc_pids();
    }

    /* Check all ports */
    if (rootcheck.checks.rc_ports) {
#ifndef WIN32
        mtdebug1(ARGV0, "Going into check_rc_ports");
        check_rc_ports();

        /* Check open ports */
        mtdebug1(ARGV0, "Going into check_open_ports");
        check_open_ports();
#endif
    }

    /* Check interfaces */
    if (rootcheck.checks.rc_if) {
        mtdebug1(ARGV0, "Going into check_rc_if");
        check_rc_if();
    }

    mtdebug1(ARGV0, "Completed with all checks.");

    /* Clean the global memory */
    {
        int li;
        for (li = 0; li <= rk_sys_count; li++) {
            if (!rk_sys_file[li] ||
                    !rk_sys_name[li]) {
                break;
            }

            free(rk_sys_file[li]);
            free(rk_sys_name[li]);
        }
    }

    /* Final message */
    time2 = time(0);
    sleep(5);

    /* Send scan ending message */
    notify_rk(ALERT_POLICY_VIOLATION, "Ending rootcheck scan.");
    mtinfo(ARGV0, "Ending rootcheck scan. Duration: %d", (int)(time2 - time1));

    mtdebug1(ARGV0, "Leaving run_rk_check");
    return;
}

void * w_rootcheck_thread(__attribute__((unused)) void * args) {

    time_t curr_time = 0;
    time_t prev_time_rk = 0;
    syscheck_config *syscheck = args;

    sleep(syscheck->tsleep * 10);

    while (1) {
        int run_now = 0;

        /* Check if syscheck should be restarted */
        run_now = os_check_restart_syscheck();
        curr_time = time(0);

        /* If time elapsed is higher than the rootcheck_time, run it */
        if (syscheck->rootcheck) {
            if (((curr_time - prev_time_rk) > rootcheck.time) || run_now) {
                run_rk_check();
                prev_time_rk = time(0);
            }
        }
        sleep(1);
    }

    return NULL;
}
