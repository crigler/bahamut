/************************************************************************
 *   IRC - Internet Relay Chat, src/hide.c
 *   Copyright (C) 2003 Lucas Madar
 *
 *   hide.c - code for hiding information
 *
 */

/* $Id$ */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "fds.h"
#include "numeric.h"

/* This is how we maintain a 'fake' list of servers */

struct fakelinkserver {
   char *name;
   char *description;
};

static Link *lserver_list = NULL;

static struct fakelinkserver *fakelinkserver_find(char *name)
{
   Link *lp;
   struct fakelinkserver *ls;

   for(lp = lserver_list; lp; lp = lp->next)
   {
      ls = (struct fakelinkserver *) lp->value.cp;
      if(mycmp(name, ls->name) == 0)
         return ls;
   }
   return NULL;
}

/*
 * Delete the entire list
 */
void fakelinkserver_reset()
{
   Link *lp;
   struct fakelinkserver *ls;

   while((lp = lserver_list))
   {
      lserver_list = lp->next;

      ls = (struct fakelinkserver *) lp->value.cp;
      MyFree(ls->name);
      MyFree(ls->description);
      MyFree(ls);
      free_link(lp);
   }
}

static void fakelinkserver_delete(char *name)
{
   Link *lp, *lpprev, *lpn;
   struct fakelinkserver *ls;

   for(lp = lserver_list, lpprev = NULL; lp; lpprev = lp, lp = lpn)
   {
      lpn = lp->next;
      ls = (struct fakelinkserver *) lp->value.cp;
      if(mycmp(name, ls->name) == 0)
      {
         if(lpprev)
            lpprev->next = lp->next;
         else
            lserver_list = lp->next;

         MyFree(ls->name);
         MyFree(ls->description);
         MyFree(ls);
         free_link(lp);
         return;
      }
   }
}

static void fakelinkserver_add(char *name, char *desc)
{
   struct fakelinkserver *ls;
   Link *lp;

   if(fakelinkserver_find(name))
      return;

   ls = (struct fakelinkserver *) MyMalloc(sizeof(struct fakelinkserver));
   ls->name = (char *) MyMalloc(strlen(name) + 1);
   strcpy(ls->name, name);
   ls->description = (char *) MyMalloc(strlen(desc) + 1);
   strcpy(ls->description, desc);

   lp = make_link();
   lp->value.cp = (char *) ls;
   lp->next = lserver_list;
   lserver_list = lp;
}

/*
 * update the server's description 
 */
void fakelinkserver_update(char *name, char *desc)
{
   struct fakelinkserver *ls;

   if(!(ls = fakelinkserver_find(name)))
      return;

   MyFree(ls->description);
   ls->description = (char *) MyMalloc(strlen(desc) + 1);
   strcpy(ls->description, desc);
}

int fakelinkscontrol(int parc, char *parv[])
{
   if(parc < 1)
      return 0;

   if(parc > 0 && mycmp(parv[0], "RESET") == 0)
   {
      fakelinkserver_reset();
      return 0;
   }

   if(parc > 1 && mycmp(parv[0], "+") == 0)
   {
      char *servername = parv[1];
      aClient *acptr = find_server(servername, NULL);
      char *desc = (parc > 2) ? parv[2] : HIDDEN_SERVER_DESC;

      if(strchr(servername, '.') == NULL)
         return 0;

      if(strchr(servername, ' ') != NULL)
         return 0;

      fakelinkserver_add(servername, acptr ? acptr->info : desc);
   }

   if(parc > 1 && mycmp(parv[0], "-") == 0)
   {
      char *servername = parv[1];

      fakelinkserver_delete(servername);
   }

   return 0;
}

void fakeserver_list(aClient *sptr)
{
   Link *lp;

   for (lp = lserver_list; lp; lp = lp->next)
   {
      struct fakelinkserver *ls = (struct fakelinkserver *) lp->value.cp;

      sendto_one(sptr, rpl_str(RPL_LINKS), me.name, sptr->name,
                 ls->name, ls->name, 0, ls->description);
   }
}

void fakeserver_sendserver(aClient *sptr)
{
   Link *lp;

   sendto_one(sptr, ":%s LINKS CONTROL RESET", me.name);

   for (lp = lserver_list; lp; lp = lp->next)
   {
      struct fakelinkserver *ls = (struct fakelinkserver *) lp->value.cp;

      sendto_one(sptr, ":%s LINKS CONTROL + %s :%s",
                 me.name, ls->name, ls->description);
   }
}

void fakeserver_clear()
{
   fakelinkserver_reset();
}

/////////////////////////////////////////////////////////////

static time_t luserslock_expiretime = -1;

int is_luserslocked() 
{
   if(luserslock_expiretime == -1)
      return 0;

   if(luserslock_expiretime <= NOW)
   {
      luserslock_expiretime = -1;
      sendto_realops("LUSERS lock has expired");
      return 0;
   }

   return 1;
}

static struct fakelusers_struct {
   int m_server;
   int m_ulined;
   int m_client;
   int m_clientmax;
   int i_count;
   int c_count;
   int s_count;
   int o_count;
   int chan_count;
   int m_total;
   int m_totalmax;
} fakelusers = {0};

static void dolocklusers()
{
   fakelusers.m_server = Count.myserver;
   fakelusers.m_ulined = Count.myulined;
   fakelusers.m_client = Count.local;
   fakelusers.m_clientmax = Count.max_loc;
   fakelusers.i_count = Count.invisi;
   fakelusers.c_count = Count.total - Count.invisi;
   fakelusers.s_count = Count.server;
   fakelusers.o_count = Count.oper;
   fakelusers.chan_count = Count.chan;
   fakelusers.m_total = Count.total;
   fakelusers.m_totalmax = Count.max_tot;
}

void send_fake_users(aClient *sptr)
{
#ifdef CLIENT_COUNT
    sendto_one(sptr, rpl_str(RPL_LOCALUSERS), me.name, sptr->name,
                   fakelusers.m_client, fakelusers.m_clientmax);
    sendto_one(sptr, rpl_str(RPL_GLOBALUSERS), me.name, sptr->name,
               fakelusers.m_total, fakelusers.m_totalmax);
#endif
}

void send_fake_lusers(aClient *sptr) 
{
#ifdef SHOW_INVISIBLE_LUSERS
   sendto_one(sptr, rpl_str(RPL_LUSERCLIENT), me.name, sptr->name,
              fakelusers.c_count, fakelusers.i_count, fakelusers.s_count);
#else
   sendto_one(sptr,
              ":%s %d %s :There are %d users on %d servers", me.name,
              RPL_LUSERCLIENT, sptr->name, fakelusers.c_count,
              fakelusers.s_count);
#endif

   if (fakelusers.o_count)
      sendto_one(sptr, rpl_str(RPL_LUSEROP),
                 me.name, sptr->name, fakelusers.o_count);

   if(fakelusers.chan_count)
      sendto_one(sptr, rpl_str(RPL_LUSERCHANNELS),
                 me.name, sptr->name, fakelusers.chan_count);

    sendto_one(sptr, rpl_str(RPL_LUSERME), me.name, sptr->name,
#ifdef HIDEULINEDSERVS
               fakelusers.m_server - fakelusers.m_ulined);
#else
               fakelusers.m_server);
#endif

#ifdef CLIENT_COUNT
    sendto_one(sptr, rpl_str(RPL_LOCALUSERS), me.name, sptr->name,
                   fakelusers.m_client, fakelusers.m_clientmax);
    sendto_one(sptr, rpl_str(RPL_GLOBALUSERS), me.name, sptr->name,
               fakelusers.m_total, fakelusers.m_totalmax);
#endif
}

int m_luserslock(aClient *cptr, aClient *sptr, int parc, char *parv[]) 
{
   char pbuf[512];

   if(!(IsULine(sptr) || IsServer(sptr)))
      return 0;

   /* LUSERSLOCK UNTIL <time> */
   if(parc > 2 && mycmp(parv[1], "UNTIL") == 0) 
   {
      time_t until = strtol(parv[2], NULL, 0);

      if(until < NOW)
         return 0;

      if(luserslock_expiretime != -1 && luserslock_expiretime < until)
      {
         sendto_realops("LUSERS lock extended by %s (%d minute duration from now)", 
                        sptr->name, 1 + (until - NOW) / 60);

         luserslock_expiretime = until;
      }
      else if(luserslock_expiretime == -1)
      {
         sendto_realops("LUSERS lock activated by %s (%d minute duration)", 
                        sptr->name, 1 + (until - NOW) / 60);
    
         luserslock_expiretime = until;
         dolocklusers();
      }
   }
   /* LUSERSLOCK CANCEL */
   else if(parc > 1 && mycmp(parv[1], "CANCEL") == 0)
   {
      if(luserslock_expiretime != -1)
      {
         luserslock_expiretime = -1;
         sendto_realops("LUSERS lock cancelled by %s", sptr->name);
      }
   }

   make_parv_copy(pbuf, parc, parv);
   sendto_serv_butone(cptr, ":%s LUSERSLOCK %s", parv[0], pbuf);

   return 0;
}

void fakelusers_sendlock(aClient *sptr)
{
   if(luserslock_expiretime == -1)
      sendto_one(sptr, ":%s LUSERSLOCK CANCEL", me.name);
   else
      sendto_one(sptr, ":%s LUSERSLOCK UNTIL %d", (int) luserslock_expiretime);
}
