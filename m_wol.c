/*
 * Copyright (c) 2011 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int wol_cvers(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_apgar(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_serial(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_verchk(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_list(Cmdoverride *anoverride, aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_joingame(aClient *cptr, aClient *sptr, int parc, char *parv[]);
Cmdoverride *_list;

int *m_wol = NULL;

#define MSG_CVERS       "CVERS"
#define MSG_APGAR       "APGAR"
#define MSG_SERIAL      "SERIAL"
#define MSG_VERCHK      "VERCHK"
#define MSG_LIST        "LIST"
#define MSG_JOINGAME    "JOINGAME"
#define TOK_NONE        NULL

static ModuleInfo *_modinfo;

DLLFUNC ModuleHeader MOD_HEADER(m_wol) =
{
    "m_wol",
    "v1.0",
    "Westwood Online support", 
    "3.2-b8-1",
    NULL 
};

DLLFUNC int MOD_INIT(m_wol)(ModuleInfo *modinfo)
{
    CommandAdd(modinfo->handle, MSG_CVERS, TOK_NONE, wol_cvers, MAXPARA, M_UNREGISTERED);
    CommandAdd(modinfo->handle, MSG_APGAR, TOK_NONE, wol_apgar, MAXPARA, M_UNREGISTERED);
    CommandAdd(modinfo->handle, MSG_SERIAL, TOK_NONE, wol_serial, MAXPARA, M_UNREGISTERED);
    CommandAdd(modinfo->handle, MSG_VERCHK, TOK_NONE, wol_verchk, MAXPARA, M_UNREGISTERED);
    CommandAdd(modinfo->handle, MSG_JOINGAME, TOK_NONE, wol_joingame, MAXPARA, M_USER);
    _modinfo = modinfo;
    return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_wol)(int module_load)
{
    _list = CmdoverrideAdd(_modinfo->handle, MSG_LIST, wol_list);
    if (_list == NULL)
    {
        sendto_realops("m_wol: Failed to override LIST");
        return MOD_FAILED;
    }
    return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_wol)(int module_unload)
{
    CmdoverrideDel(_list);
    return MOD_SUCCESS;
}

int _is_numeric(const char *str)
{
    int i,len = strlen(str);
    for (i = 0; i < len; i++)
        if (!isdigit(str[i]))
            return 0;
    return 1;
}

int wol_cvers(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    ircd_log(LOG_ERROR, "wol_cvers(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        ircd_log(LOG_ERROR, " parv[%d]: \"%s\"", i, parv[i]);
    return 0;
}

int wol_apgar(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    ircd_log(LOG_ERROR, "wol_apgar(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        ircd_log(LOG_ERROR, " parv[%d]: \"%s\"", i, parv[i]);
    return 0;
}

int wol_serial(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    ircd_log(LOG_ERROR, "wol_serial(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        ircd_log(LOG_ERROR, " parv[%d]: \"%s\"", i, parv[i]);
    return 0;
}

int wol_verchk(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    ircd_log(LOG_ERROR, "wol_verchk(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        ircd_log(LOG_ERROR, " parv[%d]: \"%s\"", i, parv[i]);
    return 0;
}

int wol_list(Cmdoverride *anoverride, aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    ircd_log(LOG_ERROR, "wol_list(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        ircd_log(LOG_ERROR, " parv[%d]: \"%s\"", i, parv[i]);

    if (parc == 3)
    {
        if (_is_numeric(parv[1]) && _is_numeric(parv[2]))
        {
            int list_type = atoi(parv[1]);
            int game_type = atoi(parv[2]);

            ircd_log(LOG_ERROR, " detected WOL LIST, returning custom list");

            sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);

            /* emulate a single RA lobby for now */
            if (list_type == 0 && game_type == 21)
            {
                sendto_one(sptr, ":%s 327 %s %s %d %d %d", me.name, parv[0], "#Lob_21_0", 0, 0, 0);
            }

            sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
            return 0;
        }
    }

    return CallCmdoverride(_list, cptr, sptr, parc, parv);
}

int wol_joingame(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    ircd_log(LOG_ERROR, "wol_joingame(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        ircd_log(LOG_ERROR, " parv[%d]: \"%s\"", i, parv[i]);

    /* FIXME: cleanup parc and parv*/
    return do_join(cptr, sptr, parc, parv);
}
