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

/*
   This WOL module uses a simple singly linked list to store channels and users
   with special information that is required in a WOL environment.

   It means that when handling a lot of users, it might cause performance
   problems and needs to use a hash map instead. Just keep that in mind.
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

#include "wol_list.h"

#define dprintf(...) ircd_log(LOG_ERROR, __VA_ARGS__)

DLLFUNC int wol_cvers(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_apgar(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_serial(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_verchk(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_list(Cmdoverride *anoverride, aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_join(Cmdoverride *anoverride, aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_joingame(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_gameopt(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int wol_startg(aClient *cptr, aClient *sptr, int parc, char *parv[]);

DLLFUNC int wol_hook_channel_create(aClient *cptr, aChannel *chptr);
DLLFUNC int wol_hook_channel_destroy(aChannel *chptr);
DLLFUNC int wol_hook_quit(aClient *cptr, char *comment);

DLLFUNC CMD_FUNC(wol_names);

Cmdoverride *_list;
Cmdoverride *_join;

int *m_wol = NULL;

#define MSG_CVERS       "CVERS"
#define MSG_APGAR       "APGAR"
#define MSG_SERIAL      "SERIAL"
#define MSG_VERCHK      "VERCHK"
#define MSG_LIST        "LIST"
#define MSG_JOINGAME    "JOINGAME"
#define MSG_GAMEOPT     "GAMEOPT"
#define MSG_STARTG      "STARTG"
#define TOK_NONE        NULL

#define RPL_LISTGAME    326
#define RPL_LISTLOBBY   327
#define RPL_VERNONREQ   379

static ModuleInfo *_modinfo;

typedef struct wol_user
{
    aClient             *p;
    int                 SKU;
    struct wol_user*    next;
} wol_user;

typedef struct wol_channel
{
    int                 type;
    int                 minUsers;
    int                 maxUsers;
    int                 tournament;
    unsigned int        reserved;
    unsigned int        ipaddr;
    unsigned int        flags;
    wol_user            *users;
    aChannel            *p;
    struct wol_channel* next;
} wol_channel;

static wol_channel *channels = NULL;
static wol_user *users = NULL;

wol_channel *wol_get_channel(aChannel *p)
{
    wol_channel *channel;

    if (p == NULL)
        return NULL;

    WOL_LIST_FOREACH(channels, channel)
    {
        if (channel->p == p)
            return channel;
    }

    return NULL;
}

wol_user *wol_get_user(aClient *p)
{
    wol_user *user;

    WOL_LIST_FOREACH(users, user)
    {
        if (user->p == p)
            return user;
    }

    return NULL;
}

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
    CommandAdd(modinfo->handle, MSG_GAMEOPT, TOK_NONE, wol_gameopt, MAXPARA, M_USER);
    CommandAdd(modinfo->handle, MSG_STARTG, TOK_NONE, wol_startg, MAXPARA, M_USER);

    HookAddEx(modinfo->handle, HOOKTYPE_CHANNEL_CREATE, wol_hook_channel_create);
    HookAddEx(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, wol_hook_channel_destroy);
    HookAddEx(modinfo->handle, HOOKTYPE_LOCAL_QUIT, wol_hook_quit);

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
    _join = CmdoverrideAdd(_modinfo->handle, MSG_JOIN, wol_join);
    if (_join == NULL)
    {
        sendto_realops("m_wol: Failed to override LIST");
        return MOD_FAILED;
    }
    return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_wol)(int module_unload)
{
    WOL_LIST_FREE(channels);
    WOL_LIST_FREE(users);

    CmdoverrideDel(_list);
    CmdoverrideDel(_join);

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
    dprintf("wol_cvers(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);

    /* this is the first WOL specific message we get from the client and is used
       to trigger WOL specific behaviour to the client */

    wol_user *user = wol_get_user(sptr);

    if (user == NULL)
    {
        user = WOL_ALLOC(sizeof(wol_user));
        user->p = sptr;
        WOL_LIST_INSERT(users, user);
    }

    user->SKU = 1;

    return 0;
}

int wol_apgar(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_apgar(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);
    return 0;
}

int wol_serial(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_serial(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);
    return 0;
}

int wol_verchk(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_verchk(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);

    if (parc < 3)
    {
        sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "VERCHK");
        return 0;
    }

    sendto_one(sptr, ":%s %d %s :none none none 1 %s NONREQ",
            me.name,
            RPL_VERNONREQ,
            parv[0],
            parv[1]);

    return 0;
}

int wol_list(Cmdoverride *anoverride, aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_list(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);

    if (parc == 3)
    {
        if (_is_numeric(parv[1]) && _is_numeric(parv[2]))
        {
            int list_type = atoi(parv[1]);
            int game_type = atoi(parv[2]);

            dprintf(" detected WOL LIST, returning custom list");

            sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);

            /* list specific game type rooms */
            if (list_type)
            {
                wol_channel *channel;
                WOL_LIST_FOREACH(channels, channel)
                {
                    if (channel->type == list_type)
                    {
                        sendto_one(sptr, ":%s %d %s %s %d %d %d %d %u %u %u::%s",
                                me.name,
                                RPL_LISTGAME,
                                parv[0],
                                channel->p->chname,
                                channel->p->users,
                                channel->maxUsers,
                                channel->type,
                                channel->tournament,
                                channel->reserved,
                                channel->ipaddr,
                                channel->flags,
                                channel->p->topic);
                    }
                }
            }
            else
            {
                /* emulate a single RA lobby for now */
                sendto_one(sptr, ":%s %d %s %s %d %d %d", me.name, RPL_LISTLOBBY, parv[0], "#Lob_21_0", 0, 0, 0);
            }

            sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
            return 0;
        }
    }

    return CallCmdoverride(_list, cptr, sptr, parc, parv);
}

int wol_join(Cmdoverride *anoverride, aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_join(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);

    wol_user    *user       = wol_get_user(cptr);

    if (user)
    {
        dprintf(" detected WOL JOIN, returning custom reply");
        aChannel    *chptr      = get_channel(sptr, parv[1], CREATE);
        wol_channel *channel    = wol_get_channel(chptr);

        /* hack when the module is reloaded and state is lost */
        if (!channel)
        {
            wol_hook_channel_create(NULL, chptr);
            channel = wol_get_channel(chptr);
        }

        dprintf(" chptr=%p, channel=%p", chptr, channel);

        /* FIXME: check if the channel is joinable */
        if (channel)
        {
            /* read in the WOL channel settings from parv */

            add_user_to_channel(chptr, sptr, 0);

            sendto_channel_butserv(chptr, sptr,
                ":%s JOIN :0,0 %s", sptr->name, chptr->chname);
            
            sendto_serv_butone_token_opt(cptr, OPT_NOT_SJ3, sptr->name, "JOIN",
                    TOK_JOIN, "%s", chptr->chname);

            if (MyClient(sptr))
            {
                del_invite(sptr, chptr);
                if (chptr->topic)
                {
                    sendto_one(sptr, rpl_str(RPL_TOPIC),
                        me.name, sptr->name, chptr->chname, chptr->topic);
                    sendto_one(sptr,
                        rpl_str(RPL_TOPICWHOTIME), me.name,
                        sptr->name, chptr->chname, chptr->topic_nick,
                        chptr->topic_time);
                }
                wol_names(cptr, sptr, 2, parv);
            }
        }
        return 0;
    }

    /* if call was forwarded from joingame, don't */
    if (anoverride == NULL)
    {
        return 0;
    }

    return CallCmdoverride(_join, cptr, sptr, parc, parv);
}

int wol_joingame(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_joingame(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);

    if (parc != 3 && parc != 4 && parc != 9)
    {
        sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "JOINGAME");
        return 0;
    }

    /* handle buggy JOIN from RA */
    if (parc == 4)
    {
        return wol_join(NULL, cptr, sptr, parc, parv);
    }

    int         flags       = (ChannelExists(parv[1])) ? CHFL_DEOPPED : LEVEL_ON_JOIN;
    aChannel    *chptr      = get_channel(sptr, parv[1], CREATE);
    wol_channel *channel    = wol_get_channel(chptr);

    if (flags == LEVEL_ON_JOIN && parc < 9)
    {
        sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "JOINGAME");
        return 0;
    }

    if (!channel && (flags != LEVEL_ON_JOIN))
    {
        dprintf(" no game channel while joining, this is a bug!");
        sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "JOINGAME");
        return 0;
    }

    dprintf(" chptr=%p, channel=%p", chptr, channel);

    /* FIXME: check if the channel is joinable */
    if (channel)
    {
        if (flags == LEVEL_ON_JOIN)
        {
            /* read in the WOL channel settings from parv */
            channel->minUsers   = atoi(parv[2]);
            channel->maxUsers   = atoi(parv[3]);
            channel->type       = atoi(parv[4]);
            channel->tournament = atoi(parv[7]);
            channel->reserved   = atoi(parv[8]);

            if (parc > 9)
            {
                // 9 == key if exists
            }
        }

        add_user_to_channel(chptr, sptr, flags);

        sendto_channel_butserv(chptr, sptr,
            ":%s JOINGAME %d %d %d %d %u %u %u :%s",
            sptr->name,
            channel->minUsers,
            channel->maxUsers,
            channel->type,
            channel->tournament,
            0, /* unk */
            0, /* host ipaddr, not used */
            0, /* unk */
            chptr->chname);
        
        if (MyClient(sptr))
        {
            del_invite(sptr, chptr);
            if (chptr->topic)
            {
                sendto_one(sptr, rpl_str(RPL_TOPIC),
                    me.name, sptr->name, chptr->chname, chptr->topic);
            }
            wol_names(cptr, sptr, 2, parv);
        }
    }

    return 0;
}

int wol_gameopt(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_gameopt(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);

    if (parc < 3)
    {
        sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "GAMEOPT");
        return 0;
    }

    if (parv[1][0] == '#')
    {
        aChannel *chptr = find_channel(parv[1], NULL);

        if (!chptr)
        {
            sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, parv[0], parv[1]);
            return 0;
        }

        sendto_channel_butserv(chptr, sptr, ":%s GAMEOPT %s :%s", sptr->name, chptr->chname, parv[2]);
    }
    else
    {
        aClient *clptr = find_person(parv[1], NULL);

        if (!clptr)
        {
            sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
            return 0;
        }

        sendto_prefix_one(clptr, sptr, ":%s GAMEOPT %s :%s", parv[0], clptr->name, parv[2]);
    }

    return 0;
}

int wol_startg(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    dprintf("wol_startg(cptr=%p, sptr=%p, parc=%d, parv=%p)", cptr, sptr, parc, parv);
    int i;
    for (i = 0; i < parc; i++)
        dprintf(" parv[%d]: \"%s\"", i, parv[i]);

    if (parc < 3)
    {
        sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "GAMEOPT");
        return 0;
    }


    aChannel *chptr = find_channel(parv[1], NULL);
    char *p;
    char *name;
    char users[512] = { 0 };
    char buf[64];
    name = strtoken(&p, parv[2], ",");
    do
    {
        aClient *clptr = find_person(name, NULL);

        if (clptr)
        {
            snprintf(buf, 64, "%s %s ", name, GetIP(clptr));
            strlcat(users, buf, sizeof(users));
        }

    } while(name = strtoken(&p, NULL, ","));

    dprintf(":%s STARTG %s :%s :%u %d", sptr->name, chptr->chname, users, 1, (int)time(NULL));
    sendto_channel_butserv(chptr, sptr, ":%s STARTG %s :%s:%u %d", sptr->name, chptr->chname, users, 1, (int)time(NULL));

    return 0;
}

DLLFUNC int wol_hook_channel_create(aClient *cptr, aChannel *chptr)
{
    dprintf("wol_hook_channel_create(cptr=%p, chptr=%p)", cptr, chptr);

    wol_channel *channel = WOL_ALLOC(sizeof(wol_channel));
    channel->p = chptr;
    WOL_LIST_INSERT(channels, channel);

    return 0;
}

DLLFUNC int wol_hook_channel_destroy(aChannel *chptr)
{
    dprintf("wol_hook_channel_destroy(chptr=%p)", chptr);
    wol_channel *channel    = wol_get_channel(chptr);

    if (channel)
    {
        WOL_LIST_REMOVE(channels, channel);
    }

    WOL_FREE(channel);

    return 0;
}

int wol_hook_quit(aClient *cptr, char *comment)
{
    dprintf("wol_hook_quit(cptr=%p, comment=\"%s\")", cptr, comment);

    wol_user    *user       = wol_get_user(cptr);

    dprintf(" users %p", users);
    dprintf(" user %p", user);

    WOL_LIST_REMOVE(users, user);
    WOL_FREE(user);

    return 0;
}

static char buf[BUFSIZE];
#define TRUNCATED_NAMES 64
DLLFUNC CMD_FUNC(wol_names)
{
    int bufLen = NICKLEN + 4; /* extra = ,0,0 */
    int  mlen = strlen(me.name) + bufLen + 7;
    aChannel *chptr;
    aClient *acptr;
    int  member;
    Member *cm;
    int  idx, flag = 1, spos;
    char *s, *para = parv[1];
    char nuhBuffer[NICKLEN+USERLEN+HOSTLEN+3];


    if (parc < 2 || !MyConnect(sptr))
    {
        sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name,
            parv[0], "*");
        return 0;
    }

    if (parc > 1 &&
        hunt_server_token(cptr, sptr, MSG_NAMES, TOK_NAMES, "%s %s", 2, parc, parv))
        return 0;

    for (s = para; *s; s++)
    {
        if (*s == ',')
        {
            if (strlen(para) > TRUNCATED_NAMES)
                para[TRUNCATED_NAMES] = '\0';
            sendto_realops("names abuser %s %s",
                get_client_name(sptr, FALSE), para);
            sendto_one(sptr, err_str(ERR_TOOMANYTARGETS),
                me.name, sptr->name, "NAMES");
            return 0;
        }
    }

    chptr = find_channel(para, (aChannel *)NULL);

    if (!chptr || (!ShowChannel(sptr, chptr) && !OPCanSeeSecret(sptr)))
    {
        sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name,
            parv[0], para);
        return 0;
    }

    /* cache whether this user is a member of this channel or not */
    member = IsMember(sptr, chptr);

    if (PubChannel(chptr))
        buf[0] = '=';
    else if (SecretChannel(chptr))
        buf[0] = '@';
    else
        buf[0] = '*';

    idx = 1;
    buf[idx++] = ' ';
    for (s = chptr->chname; *s; s++)
        buf[idx++] = *s;
    buf[idx++] = ' ';
    buf[idx++] = ':';

    /* If we go through the following loop and never add anything,
       we need this to be empty, otherwise spurious things from the
       LAST /names call get stuck in there.. - lucas */
    buf[idx] = '\0';

    spos = idx;        /* starting point in buffer for names! */

    for (cm = chptr->members; cm; cm = cm->next)
    {
        acptr = cm->cptr;
        if (IsInvisible(acptr) && !member && !IsNetAdmin(sptr))
            continue;

        if (cm->flags & CHFL_CHANOP)
                buf[idx++] = '@';
        else if (cm->flags & CHFL_VOICE)
                buf[idx++] = '+';

        s = acptr->name;

        for (; *s; s++)
            buf[idx++] = *s;

        /* WOL addition */
        buf[idx++] = ',';
        buf[idx++] = '0';
        buf[idx++] = ',';
        buf[idx++] = '0';

        buf[idx++] = ' ';
        buf[idx] = '\0';

        flag = 1;
        if (mlen + idx + bufLen > BUFSIZE - 7)
        {
            sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name,
                parv[0], buf);
            idx = spos;
            flag = 0;
        }
    }

    if (flag)
        sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name, parv[0], buf);

    sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0], para);

    return 0;

}
