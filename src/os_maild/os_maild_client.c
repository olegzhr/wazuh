/* Copyright (C) 2015-2020, Wazuh Inc.
 * Copyright (C) 2009 Trend Micro Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

#include "shared.h"
#include "maild.h"
#include <external/cJSON/cJSON.h>
#ifdef LIBGEOIP_ENABLED
#include "config/config.h"
#endif

/**
 * @brief Function to add a json field to the alert buffer.
 * @param json_object JSON object that where the field will be looked for.
 * @param logs Alert buffer.
 * @param body_size Size of the buffer.
 * @param field Field to look for.
 * @param field_name Name that will be used for the field in the alert.
 */
void add_field_from_json(cJSON *json_object, char *logs, size_t *body_size, char *field, char *field_name) {
    cJSON *json_field;
    size_t log_size = 0;
    json_field = cJSON_GetObjectItem(json_object, field);
    if (json_field) {
        log_size = strlen(json_field->valuestring) + strlen(field_name) + 3;
        if (*body_size > log_size) {
            strcat(logs, field_name);
            strncat(logs, json_field->valuestring, *body_size);
            strcat(logs, "\r\n");
            *(body_size) -= log_size;
        }
    }
}

/* Receive a Message on the Mail queue */
MailMsg *OS_RecvMailQ(file_queue *fileq, struct tm *p, MailConfig *Mail, MailMsg **msg_sms)
{
    int i = 0, sms_set = 0, donotgroup = 0;
    size_t body_size = OS_MAXSTR - 3, log_size;
    char logs[OS_MAXSTR + 1];
    char extra_data[OS_MAXSTR + 1];
    char log_string[OS_MAXSTR / 4 + 1];
    char *subject_host;
#ifdef LIBGEOIP_ENABLED
    char geoip_msg_src[OS_SIZE_1024 + 1];
    char geoip_msg_dst[OS_SIZE_1024 + 1];
#endif

    MailMsg *mail;
    alert_data *al_data;

    Mail->priority = 0;

    /* Get message if available */
    al_data = Read_FileMon(fileq, p, mail_timeout);
    if (!al_data) {
        return (NULL);
    }

    /* If e-mail came correctly, generate the e-mail body/subject */
    os_calloc(1, sizeof(MailMsg), mail);
    os_calloc(BODY_SIZE, sizeof(char), mail->body);
    os_calloc(SUBJECT_SIZE, sizeof(char), mail->subject);

    /* Generate the logs */
    logs[0] = '\0';
    extra_data[0] = '\0';
    logs[OS_MAXSTR] = '\0';

    while (al_data->log[i]) {
        log_size = strlen(al_data->log[i]) + 4;

        /* If size left is small than the size of the log, stop it */
        if (body_size <= log_size) {
            break;
        }

        strncat(logs, al_data->log[i], body_size);
        strncat(logs, "\r\n", body_size);
        body_size -= log_size;
        i++;
    }

    if (al_data->old_md5) {
        log_size = strlen(al_data->old_md5) + 16 + 4;
        if (body_size > log_size) {
            strcat(logs, "Old md5sum was: ");
            strncat(logs, al_data->old_md5, body_size);
            strcat(logs, "\r\n");
            body_size -= log_size;
        }
    }
    if (al_data->new_md5) {
        log_size = strlen(al_data->new_md5) + 16 + 4;
        if (body_size > log_size) {
            strcat(logs, "New md5sum is : ");
            strncat(logs, al_data->new_md5, body_size);
            strcat(logs, "\r\n");
            body_size -= log_size;
        }
    }
    if (al_data->old_sha1) {
        log_size = strlen(al_data->old_sha1) + 17 + 4;
        if (body_size > log_size) {
            strcat(logs, "Old sha1sum was: ");
            strncat(logs, al_data->old_sha1, body_size);
            strcat(logs, "\r\n");
            body_size -= log_size;
        }
    }
    if (al_data->new_sha1) {
        log_size = strlen(al_data->new_sha1) + 17 + 4;
        if (body_size > log_size) {
            strcat(logs, "New sha1sum is : ");
            strncat(logs, al_data->new_sha1, body_size);
            strcat(logs, "\r\n");
            body_size -= log_size;
        }
    }
    if (al_data->old_sha256) {
        log_size = strlen(al_data->old_sha256) + 19 + 4;
        if (body_size > log_size) {
            strcat(logs, "Old sha256sum was: ");
            strncat(logs, al_data->old_sha256, body_size);
            strcat(logs, "\r\n");
            body_size -= log_size;
        }
    }
    if (al_data->new_sha256) {
        log_size = strlen(al_data->new_sha256) + 19 + 4;
        if (body_size > log_size) {
            strcat(logs, "New sha256sum is : ");
            strncat(logs, al_data->new_sha256, body_size);
            strcat(logs, "\r\n");
            body_size -= log_size;
        }
    }

    /* EXTRA DATA */
    if (al_data->srcip) {
        log_size = snprintf(log_string, sizeof(log_string) - 1, "Src IP: %s\r\n", al_data->srcip );
        if (body_size > log_size) {
            if ( strncat(extra_data, log_string, log_size) != NULL ) {
                body_size -= log_size;
            }
        }
    }
    if (al_data->dstip) {
        log_size = snprintf(log_string, sizeof(log_string) - 1, "Dst IP: %s\r\n", al_data->dstip );
        if (body_size > log_size) {
            if ( strncat(extra_data, log_string, log_size) != NULL ) {
                body_size -= log_size;
            }
        }
    }
    if (al_data->user) {
        log_size = snprintf(log_string, sizeof(log_string) - 1, "User: %s\r\n", al_data->user );
        if (body_size > log_size) {
            strncat(extra_data, log_string, log_size);
        }
    }

    /* Subject */
    subject_host = strchr(al_data->location, '>');
    if (subject_host) {
        subject_host--;
        *subject_host = '\0';
    }

    /* We have two subject options - full and normal */
    if (Mail->subject_full) {
        /* Option for a clean full subject (without ossec in the name) */
#ifdef CLEANFULL
        snprintf(mail->subject, SUBJECT_SIZE - 1, MAIL_SUBJECT_FULL2,
                 al_data->level,
                 al_data->comment,
                 al_data->location);
#else
        snprintf(mail->subject, SUBJECT_SIZE - 1, MAIL_SUBJECT_FULL,
                 al_data->location,
                 al_data->level,
                 al_data->comment);
#endif
    } else {
        snprintf(mail->subject, SUBJECT_SIZE - 1, MAIL_SUBJECT,
                 al_data->location,
                 al_data->level);
    }


    /* Fix subject back */
    if (subject_host) {
        *subject_host = '-';
    }

#ifdef LIBGEOIP_ENABLED
    /* Get GeoIP information */
    if (Mail->geoip) {
        if (al_data->srcgeoip) {
            snprintf(geoip_msg_src, OS_SIZE_1024, "Src Location: %s\r\n", al_data->srcgeoip);
        } else {
            geoip_msg_src[0] = '\0';
        }
        if (al_data->dstgeoip) {
            snprintf(geoip_msg_dst, OS_SIZE_1024, "Dst Location: %s\r\n", al_data->dstgeoip);
        } else {
            geoip_msg_dst[0] = '\0';
        }
    } else {
        geoip_msg_src[0] = '\0';
        geoip_msg_dst[0] = '\0';
    }
#endif

    /* Body */
#ifdef LIBGEOIP_ENABLED
    snprintf(mail->body, BODY_SIZE - 1, MAIL_BODY,
             al_data->date,
             al_data->location,
             al_data->rule,
             al_data->level,
             al_data->comment,
             geoip_msg_src,
             geoip_msg_dst,
             extra_data,
             logs);
#else
    os_snprintf(mail->body, BODY_SIZE - 1, MAIL_BODY,
                al_data->date,
                al_data->location,
                al_data->rule,
                al_data->level,
                al_data->comment,
                extra_data,
                logs);
#endif
    mdebug2("OS_RecvMailQ: mail->body[%s]", mail->body);

    /* Check for granular email configs */
    if (Mail->gran_to) {
        i = 0;
        while (Mail->gran_to[i] != NULL) {
            int gr_set = 0;

            /* Look if location is set */
            if (Mail->gran_location[i]) {
                if (OSMatch_Execute(al_data->location,
                                    strlen(al_data->location),
                                    Mail->gran_location[i])) {
                    gr_set = 1;
                } else {
                    i++;
                    continue;
                }
            }

            /* Look for the level */
            if (Mail->gran_level[i]) {
                if (al_data->level >= Mail->gran_level[i]) {
                    gr_set = 1;
                } else {
                    i++;
                    continue;
                }
            }

            /* Look for rule id */
            if (Mail->gran_id[i]) {
                int id_i = 0;
                while (Mail->gran_id[i][id_i] != 0) {
                    if (Mail->gran_id[i][id_i] == al_data->rule) {
                        break;
                    }
                    id_i++;
                }

                /* If we found, id is going to be a valid rule */
                if (Mail->gran_id[i][id_i]) {
                    gr_set = 1;
                } else {
                    i++;
                    continue;
                }
            }

            /* Look for the group */
            if (Mail->gran_group[i]) {
                if (al_data->group && OSMatch_Execute(al_data->group,
                                    strlen(al_data->group),
                                    Mail->gran_group[i])) {
                    gr_set = 1;
                } else {
                    i++;
                    continue;
                }
            }

            /* If we got here, everything matched. Set this e-mail to be used. */
            if (gr_set) {
                if (Mail->gran_format[i] == SMS_FORMAT) {
                    Mail->gran_set[i] = SMS_FORMAT;

                    /* Set the SMS flag */
                    sms_set = 1;
                } else {
                    /* Options */
                    if (Mail->gran_format[i] == FORWARD_NOW) {
                        Mail->priority = 1;
                        Mail->gran_set[i] = FULL_FORMAT;
                    } else if (Mail->gran_format[i] == DONOTGROUP) {
                        Mail->priority = DONOTGROUP;
                        Mail->gran_set[i] = DONOTGROUP;
                        donotgroup = 1;
                    } else {
                        Mail->gran_set[i] = FULL_FORMAT;
                    }
                }
            }
            i++;
        }
    }


    /* If DONOTGROUP is set, we can't assign the new subject */
    if (!donotgroup) {
        /* Get highest level for alert */
        if (_g_subject[0] != '\0') {
            if (_g_subject_level < al_data->level) {
                strncpy(_g_subject, mail->subject, SUBJECT_SIZE);
                _g_subject_level = al_data->level;
            }
        } else {
            strncpy(_g_subject, mail->subject, SUBJECT_SIZE);
            _g_subject_level = al_data->level;
        }
    }

    /* If SMS is set, create the SMS output */
    if (sms_set) {
        MailMsg *msg_sms_tmp;

        /* Allocate memory for SMS */
        os_calloc(1, sizeof(MailMsg), msg_sms_tmp);
        os_calloc(BODY_SIZE, sizeof(char), msg_sms_tmp->body);
        os_calloc(SUBJECT_SIZE, sizeof(char), msg_sms_tmp->subject);

        snprintf(msg_sms_tmp->subject, SUBJECT_SIZE - 1, SMS_SUBJECT,
                 al_data->level,
                 al_data->rule,
                 al_data->comment);


        strncpy(msg_sms_tmp->body, logs, 128);
        msg_sms_tmp->body[127] = '\0';
        *msg_sms = msg_sms_tmp;
    }

    /* Clear the memory */
    FreeAlertData(al_data);

    return (mail);
}

MailMsg *OS_RecvMailQ_JSON(file_queue *fileq, MailConfig *Mail, MailMsg **msg_sms)
{
    int i = 0, sms_set = 0, donotgroup = 0;
    size_t body_size = OS_MAXSTR - 3, log_size;
    char logs[OS_MAXSTR + 1] = "";
    char *subject_host = NULL;
    char *json_str;
    int end_ok = 0;
    unsigned int alert_level = 0;
    char *alert_desc = NULL;
    char *timestamp = NULL;
    unsigned int rule_id = 0;

    MailMsg *mail = NULL;
    cJSON *al_json;
    cJSON *json_object;
    cJSON *json_audit;
    cJSON *json_field;
    cJSON *location;
    cJSON *agent;
    cJSON *agent_name;
    cJSON *agent_ip;
    cJSON *rule;
    cJSON *mail_flag;

    Mail->priority = 0;

    /* Get message if available */
    if (al_json = jqueue_next(fileq), !al_json) {
        return NULL;
    }

    if (!(rule = cJSON_GetObjectItem(al_json, "rule"), rule && (mail_flag = cJSON_GetObjectItem(rule, "mail"), mail_flag && cJSON_IsTrue(mail_flag))))
        goto end;

    /* If e-mail came correctly, generate the e-mail body/subject */
    os_calloc(1, sizeof(MailMsg), mail);
    os_calloc(BODY_SIZE, sizeof(char), mail->body);
    os_calloc(SUBJECT_SIZE, sizeof(char), mail->subject);


    /* Add alert to logs */

    if (json_object = cJSON_GetObjectItem(al_json,"syscheck"), json_object){

        add_field_from_json(json_object, logs, &body_size, "path", "- File: ");
        add_field_from_json(json_object, logs, &body_size, "event", "- Event: ");
        add_field_from_json(json_object, logs, &body_size, "mode", "- Mode: ");
        add_field_from_json(json_object, logs, &body_size, "size_before", "- Size before: ");
        add_field_from_json(json_object, logs, &body_size, "size_after", "- Size after: ");
        add_field_from_json(json_object, logs, &body_size, "md5_before", "- Old md5sum was: ");
        add_field_from_json(json_object, logs, &body_size, "md5_after", "- New md5sum is: ");
        add_field_from_json(json_object, logs, &body_size, "sha1_before", "- Old sha1sum was: ");
        add_field_from_json(json_object, logs, &body_size, "sha1_after", "- New sha1sum is: ");
        add_field_from_json(json_object, logs, &body_size, "sha256_before", "- Old sha256sum was: ");
        add_field_from_json(json_object, logs, &body_size, "sha256_after", "- New sha256sum is: ");

        // get audit information
        if (json_audit = cJSON_GetObjectItem(json_object,"audit"), json_audit){
            strcat(logs, "\nAudit information:\n");

            json_field = cJSON_GetObjectItem(json_audit,"user");
            if (json_field) {
                add_field_from_json(json_field, logs, &body_size, "name", "- (Audit) User name: ");
            }

            json_field = cJSON_GetObjectItem(json_audit,"login_user");
            if (json_field) {
                add_field_from_json(json_field, logs, &body_size, "name", "- (Audit) Audit name: ");
            }

            json_field = cJSON_GetObjectItem(json_audit,"effective_user");
            if (json_field) {
                add_field_from_json(json_field, logs, &body_size, "name", "- (Audit) Effective name: ");
            }

            json_field = cJSON_GetObjectItem(json_audit,"group");
            if (json_field) {
                add_field_from_json(json_field, logs, &body_size, "name", "- (Audit) Group name: ");
            }

            json_field = cJSON_GetObjectItem(json_audit,"process");
            if (json_field) {
                add_field_from_json(json_field, logs, &body_size, "id", "- (Audit) Process id: ");
                add_field_from_json(json_field, logs, &body_size, "name", "- (Audit) Process name: ");
                add_field_from_json(json_field, logs, &body_size, "cwd", "- (Audit) Process cwd: ");
                add_field_from_json(json_field, logs, &body_size, "parent_name", "- (Audit) Parent process name: ");
                add_field_from_json(json_field, logs, &body_size, "ppid", "- (Audit) Parent process id: ");
                add_field_from_json(json_field, logs, &body_size, "parent_cwd", "- (Audit) Parent process cwd: ");
            }
        }

        add_field_from_json(json_object, logs, &body_size, "diff", "\n- Changed content:\n");

    } else if(json_field = cJSON_GetObjectItem(al_json,"full_log"), json_field){

        log_size = strlen(json_field->valuestring) + 4;

        if (body_size <= log_size) {
            goto end;
        }

        strncpy(logs, json_field->valuestring, body_size);
        strncpy(logs + log_size, "\r\n", body_size - log_size);

    } else {
        /* The full alert is printed */
        /* tab is used to determine the number of tabs on each line */
        char *tab;
        os_malloc(256*sizeof(char), tab);
        strncpy(tab, "\t", 2);

        PrintTable(al_json, logs, body_size, tab, 2);

        free(tab);
    }


    /* Subject */

    if (location = cJSON_GetObjectItem(al_json, "location"), !location) {
        goto end;
    }

    if (agent = cJSON_GetObjectItem(al_json, "agent"), !agent) {
        goto end;
    }

    if (agent_name = cJSON_GetObjectItem(agent, "name"), !agent_name) {
        goto end;
    }

    if (agent_ip = cJSON_GetObjectItem(agent, "ip"), agent_ip) {
        os_malloc(strlen(agent_name->valuestring) + strlen(agent_ip->valuestring) + strlen(location->valuestring) + 6, subject_host);
        sprintf(subject_host, "(%s) %s->%s", agent_name->valuestring, agent_ip->valuestring, location->valuestring);
    } else {
        os_malloc(strlen(agent_name->valuestring) + strlen(location->valuestring) + 3, subject_host);
        sprintf(subject_host, "%s->%s", agent_name->valuestring, location->valuestring);
    }

    if (json_field = cJSON_GetObjectItem(rule,"level"), !json_field) {
        goto end;
    }
    alert_level = json_field->valueint;

    if (json_field = cJSON_GetObjectItem(rule,"description"), !json_field) {
        goto end;
    }
    alert_desc = strdup(json_field->valuestring);

    if (json_field = cJSON_GetObjectItem(rule,"id"), !json_field) {
        goto end;
    }
    rule_id = atoi(json_field->valuestring);

    /* We have two subject options - full and normal */
    if (Mail->subject_full) {
        /* Option for a clean full subject (without ossec in the name) */
#ifdef CLEANFULL
        snprintf(mail->subject, SUBJECT_SIZE - 1, MAIL_SUBJECT_FULL2,
                 alert_level,
                 alert_desc,
                 subject_host);
#else
        snprintf(mail->subject, SUBJECT_SIZE - 1, MAIL_SUBJECT_FULL,
                 subject_host,
                 alert_level,
                 alert_desc);
#endif
    } else {
        snprintf(mail->subject, SUBJECT_SIZE - 1, MAIL_SUBJECT,
                 subject_host,
                 alert_level);
    }

    json_field = cJSON_GetObjectItem(al_json,"timestamp");
    if (!json_field) {
        goto end;
    }
    timestamp = strdup(json_field->valuestring);

    /* Body */
    snprintf(mail->body, BODY_SIZE - 1, MAIL_BODY,
             timestamp,
             subject_host,
             rule_id,
             alert_level,
             alert_desc,
             "",
             logs);

    mdebug2("OS_RecvMailQ: mail->body[%s]", mail->body);

    /* Check for granular email configs */
    if (Mail->gran_to) {
        i = 0;
        while (Mail->gran_to[i] != NULL) {
            int gr_set = 0;

            /* Look if location is set */
            if (Mail->gran_location[i]) {
                if (OSMatch_Execute(subject_host,
                                    strlen(subject_host),
                                    Mail->gran_location[i])) {
                    gr_set = 1;
                } else {
                    i++;
                    continue;
                }
            }

            /* Look for the level */
            if (Mail->gran_level[i]) {
                if (alert_level >= Mail->gran_level[i]) {
                    gr_set = 1;
                } else {
                    i++;
                    continue;
                }
            }

            /* Look for rule id */
            if (Mail->gran_id[i]) {
                int id_i = 0;
                while (Mail->gran_id[i][id_i] != 0) {
                    if (Mail->gran_id[i][id_i] == rule_id) {
                        break;
                    }
                    id_i++;
                }

                /* If we found, id is going to be a valid rule */
                if (Mail->gran_id[i][id_i]) {
                    gr_set = 1;
                } else {
                    i++;
                    continue;
                }
            }

            /* Look for the group */
            if (json_object = cJSON_GetObjectItem(rule,"group"), json_object) {
                int found = 0;

                if (Mail->gran_group[i]) {
                    cJSON_ArrayForEach(json_field, json_object) {
                        json_str = json_field->valuestring;
                        if (OSMatch_Execute(json_str, strlen(json_str), Mail->gran_group[i])) {
                            found++;
                        }
                    }
                    if (!found) {
                        i++;
                        continue;
                    }else{
                        gr_set = 1;
                    }
                }
            }

            /* If we got here, everything matched. Set this e-mail to be used. */
            if (gr_set) {
                if (Mail->gran_format[i] == SMS_FORMAT) {
                    Mail->gran_set[i] = SMS_FORMAT;

                    /* Set the SMS flag */
                    sms_set = 1;
                } else {
                    /* Options */
                    if (Mail->gran_format[i] == FORWARD_NOW) {
                        Mail->priority = 1;
                        Mail->gran_set[i] = FULL_FORMAT;
                    } else if (Mail->gran_format[i] == DONOTGROUP) {
                        Mail->priority = DONOTGROUP;
                        Mail->gran_set[i] = DONOTGROUP;
                        donotgroup = 1;
                    } else {
                        Mail->gran_set[i] = FULL_FORMAT;
                    }
                }
            }
            i++;
        }
    }


    /* If DONOTGROUP is set, we can't assign the new subject */
    if (!donotgroup) {
        /* Get highest level for alert */
        if (_g_subject[0] != '\0') {
            if (_g_subject_level < alert_level) {
                strncpy(_g_subject, mail->subject, SUBJECT_SIZE);
                _g_subject_level = alert_level;
            }
        } else {
            strncpy(_g_subject, mail->subject, SUBJECT_SIZE);
            _g_subject_level = alert_level;
        }
    }

    /* If SMS is set, create the SMS output */
    if (sms_set) {
        MailMsg *msg_sms_tmp;

        /* Allocate memory for SMS */
        os_calloc(1, sizeof(MailMsg), msg_sms_tmp);
        os_calloc(BODY_SIZE, sizeof(char), msg_sms_tmp->body);
        os_calloc(SUBJECT_SIZE, sizeof(char), msg_sms_tmp->subject);

        snprintf(msg_sms_tmp->subject, SUBJECT_SIZE - 1, SMS_SUBJECT,
                 alert_level,
                 rule_id,
                 alert_desc);


        strncpy(msg_sms_tmp->body, logs, 128);
        msg_sms_tmp->body[127] = '\0';
        *msg_sms = msg_sms_tmp;
    }

    end_ok = 1;

end:

    /* Clear the memory */
    cJSON_Delete(al_json);
    free(alert_desc);
    free(timestamp);
    free(subject_host);

    if (end_ok) {
        return mail;
    } else if (mail) {
        free(mail->body);
        free(mail->subject);
        free(mail);
    }

    return NULL;
}

/* Read cJSON and save in printed with email format */
void PrintTable(cJSON *item, char *printed, size_t body_size, char *tab, int counter)
{
    char *key;
    size_t log_size;
    char *tab_child;
    int max_tabs = 12;
    char *delimitator = ": ";
    char *endline = "\r\n";
    char *space = " ";

    /* Like tab, tab_child is used to derterminate the number of times a line must be tabbed. */
    os_malloc(256*sizeof(char), tab_child);
    strncpy(tab_child, tab, 256*sizeof(char));


    /* If final node, it print */
    if ((item->type & 0xFF) == cJSON_Number || (item->type & 0xFF) == cJSON_String ||
        (item->type & 0xFF) == cJSON_False || (item->type & 0xFF) == cJSON_True ){

        item->string[0] = toupper(item->string[0]);
        key = cJSON_PrintUnformatted(item);
        log_size = strlen(key) + strlen(tab) + strlen(item->string) + strlen(delimitator) + strlen(endline);

        if (body_size > log_size) {
            strncat(printed, tab, strlen(tab));
            strncat(printed, item->string, strlen(item->string));
            strncat(printed, delimitator, strlen(delimitator));
            strncat(printed, key, body_size);
            strncat(printed, endline, strlen(endline));
            body_size -= log_size;
        }

        free(key);
    }
    else if ((item->type & 0xFF) == cJSON_Array){

        cJSON *json_array;
        int i = 0;
        log_size = strlen(item->string) + strlen(tab) + strlen(delimitator);

        if(body_size > log_size){
            item->string[0] = toupper(item->string[0]);
            strncat(printed, tab, strlen(tab));
            strncat(printed, item->string, strlen(item->string));
            strncat(printed, delimitator, strlen(delimitator));
            body_size -= log_size;
        }

        while(json_array = cJSON_GetArrayItem(item, i), json_array){
            key = cJSON_PrintUnformatted(json_array);
            log_size = strlen(key) + strlen(space);

            if(body_size > log_size){
                strncat(printed, json_array->valuestring, body_size);
                strncat(printed, space, strlen(space));
                body_size -= log_size;
            }

            free(key);
            i++;
        }

        if(body_size > strlen(endline)){
            strncat(printed, endline, strlen(endline));
            body_size -= log_size;
        }

    }
    /* If it have a child, the PrintTable function is called with one more tabulation.*/
    else {
        if (item->child){

            if (item->string) {
                log_size = strlen(item->string) + strlen(tab) + strlen(endline);

                if (body_size > log_size) {
                    item->string[0] = toupper(item->string[0]);
                    strncat(printed, tab, strlen(tab));
                    strncat(printed, item->string, strlen(item->string));
                    strncat(printed, endline, strlen(endline));
                    body_size -= log_size;
                }
            }
            /*Cannot be tabulated more than 6 times in the message */
            if(counter < max_tabs){
                strncat(tab_child, "\t", 2);
                PrintTable(item->child, printed, body_size, tab_child, (counter + 2));
            }
            else {
                PrintTable(item->next, printed, body_size, tab, counter);
            }
        }
    }


    /* If there are more items in the array the function is called with the same number of tabs */
    if(item->next && body_size > 2){
        PrintTable(item->next, printed, body_size, tab, counter);
    }

    /* Clear memory */
    free(tab_child);
}
