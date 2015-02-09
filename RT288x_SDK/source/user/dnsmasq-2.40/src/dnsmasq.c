/* dnsmasq is Copyright (c) 2000-2006 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#include "dnsmasq.h"

struct daemon *daemon;

static char *compile_opts = 
#ifndef HAVE_IPV6
"no-"
#endif
"IPv6 "
#ifndef HAVE_GETOPT_LONG
"no-"
#endif
"GNU-getopt "
#ifdef HAVE_BROKEN_RTC
"no-RTC "
#endif
#ifdef NO_FORK
"no-MMU "
#endif
#ifndef HAVE_ISC_READER
"no-"
#endif
"ISC-leasefile "
#ifndef HAVE_DBUS
"no-"
#endif
"DBus "
#ifdef NO_GETTEXT
"no-"
#endif
"I18N "
#ifndef HAVE_TFTP
"no-"
#endif
"TFTP";

static volatile pid_t pid = 0;
static volatile int pipewrite;

static int set_dns_listeners(time_t now, fd_set *set, int *maxfdp);
static void check_dns_listeners(fd_set *set, time_t now);
static void sig_handler(int sig);
static void async_event(int pipe, time_t now);
static void poll_resolv(void);

int main (int argc, char **argv)
{
  int bind_fallback = 0;
  int bad_capabilities = 0;
  time_t now, last = 0;
  struct sigaction sigact;
  struct iname *if_tmp;
  int piperead, pipefd[2];
  struct passwd *ent_pw;
  long i, max_fd = sysconf(_SC_OPEN_MAX);

#ifndef NO_GETTEXT
  setlocale(LC_ALL, "");
  bindtextdomain("dnsmasq", "/usr/share/locale"); 
  textdomain("dnsmasq");
#endif

  sigact.sa_handler = sig_handler;
  sigact.sa_flags = 0;
  sigemptyset(&sigact.sa_mask);
  sigaction(SIGUSR1, &sigact, NULL);
  sigaction(SIGUSR2, &sigact, NULL);
  sigaction(SIGHUP, &sigact, NULL);
  sigaction(SIGTERM, &sigact, NULL);
  sigaction(SIGALRM, &sigact, NULL);
  sigaction(SIGCHLD, &sigact, NULL);

  /* ignore SIGPIPE */
  sigact.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sigact, NULL);

  umask(022); /* known umask, create leases and pid files as 0644 */

  read_opts(argc, argv, compile_opts);
    
  if (daemon->edns_pktsz < PACKETSZ)
    daemon->edns_pktsz = PACKETSZ;
  daemon->packet_buff_sz = daemon->edns_pktsz > DNSMASQ_PACKETSZ ? 
    daemon->edns_pktsz : DNSMASQ_PACKETSZ;
  daemon->packet = safe_malloc(daemon->packet_buff_sz);
  
  if (!daemon->lease_file)
    {
      if (daemon->dhcp)
	daemon->lease_file = LEASEFILE;
    }
#ifndef HAVE_ISC_READER
  else if (!daemon->dhcp)
    die(_("ISC dhcpd integration not available: set HAVE_ISC_READER in src/config.h"), NULL, EC_BADCONF);
#endif
  
  /* Close any file descriptors we inherited apart from std{in|out|err} */
  for (i = 0; i < max_fd; i++)
    if (i != STDOUT_FILENO && i != STDERR_FILENO && i != STDIN_FILENO)
      close(i);

#ifdef HAVE_LINUX_NETWORK
  netlink_init();
#elif !(defined(IP_RECVDSTADDR) && \
	defined(IP_RECVIF) && \
	defined(IP_SENDSRCADDR))
  if (!(daemon->options & OPT_NOWILD))
    {
      bind_fallback = 1;
      daemon->options |= OPT_NOWILD;
    }
#endif

#ifndef HAVE_TFTP
  if (daemon->options & OPT_TFTP)
    die(_("TFTP server not available: set HAVE_TFTP in src/config.h"), NULL, EC_BADCONF);
#endif

  now = dnsmasq_time();
  
  if (daemon->dhcp)
    {
#if !defined(HAVE_LINUX_NETWORK) && !defined(IP_RECVIF)
      int c;
      struct iname *tmp;
      for (c = 0, tmp = daemon->if_names; tmp; tmp = tmp->next)
	if (!tmp->isloop)
	  c++;
      if (c != 1)
	die(_("must set exactly one interface on broken systems without IP_RECVIF"), NULL, EC_BADCONF);
#endif
      /* Note that order matters here, we must call lease_init before
	 creating any file descriptors which shouldn't be leaked
	 to the lease-script init process. */
      lease_init(now);
      dhcp_init();
    }

  if (!enumerate_interfaces())
    die(_("failed to find list of interfaces: %s"), NULL, EC_MISC);
    
  if (daemon->options & OPT_NOWILD) 
    {
      daemon->listeners = create_bound_listeners();

      for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
	if (if_tmp->name && !if_tmp->used)
	  die(_("unknown interface %s"), if_tmp->name, EC_BADNET);
  
      for (if_tmp = daemon->if_addrs; if_tmp; if_tmp = if_tmp->next)
	if (!if_tmp->used)
	  {
	    prettyprint_addr(&if_tmp->addr, daemon->namebuff);
	    die(_("no interface with address %s"), daemon->namebuff, EC_BADNET);
	  }
    }
  else if (!(daemon->listeners = create_wildcard_listeners()))
    die(_("failed to create listening socket: %s"), NULL, EC_BADNET);
  
  cache_init();

  if (daemon->options & OPT_DBUS)
#ifdef HAVE_DBUS
    {
      char *err;
      daemon->dbus = NULL;
      daemon->watches = NULL;
      if ((err = dbus_init()))
	die(_("DBus error: %s"), err, EC_MISC);
    }
#else
  die(_("DBus not available: set HAVE_DBUS in src/config.h"), NULL, EC_BADCONF);
#endif
  
  /* If query_port is set then create a socket now, before dumping root
     for use to access nameservers without more specific source addresses.
     This allows query_port to be a low port */
  if (daemon->query_port)
    {
      union  mysockaddr addr;
      memset(&addr, 0, sizeof(addr));
      addr.in.sin_family = AF_INET;
      addr.in.sin_addr.s_addr = INADDR_ANY;
      addr.in.sin_port = htons(daemon->query_port);
#ifdef HAVE_SOCKADDR_SA_LEN
      addr.in.sin_len = sizeof(struct sockaddr_in);
#endif
      allocate_sfd(&addr, &daemon->sfds);
#ifdef HAVE_IPV6
      memset(&addr, 0, sizeof(addr));
      addr.in6.sin6_family = AF_INET6;
      addr.in6.sin6_addr = in6addr_any;
      addr.in6.sin6_port = htons(daemon->query_port);
#ifdef HAVE_SOCKADDR_SA_LEN
      addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
      allocate_sfd(&addr, &daemon->sfds);
#endif
    }
  
  /* Use a pipe to carry signals and other events back to the event loop 
     in a race-free manner */
  if (pipe(pipefd) == -1 || !fix_fd(pipefd[0]) || !fix_fd(pipefd[1])) 
    die(_("cannot create pipe: %s"), NULL, EC_MISC);
  
  piperead = pipefd[0];
  pipewrite = pipefd[1];
  /* prime the pipe to load stuff first time. */
  send_event(pipewrite, EVENT_RELOAD, 0); 
  
  if (!(daemon->options & OPT_DEBUG))   
    {
      FILE *pidfile;
      int nullfd;

      /* The following code "daemonizes" the process. 
	 See Stevens section 12.4 */

#ifndef NO_FORK      
      if (!(daemon->options & OPT_NO_FORK))
	{
	  pid_t pid;
	  
	  if ((pid = fork()) == -1 )
	    die(_("cannot fork into background: %s"), NULL, EC_MISC);
	  
	  if (pid != 0)
	    _exit(EC_GOOD);
	  
	  setsid();
	  pid = fork();

	  if (pid != 0 && pid != -1)
	    _exit(0);
	}
#endif
      
      chdir("/");
            
      /* write pidfile _after_ forking ! */
      if (daemon->runfile && (pidfile = fopen(daemon->runfile, "w")))
      	{
	  fprintf(pidfile, "%d\n", (int) getpid());
	  fclose(pidfile);
	}
      
      /* open  stdout etc to /dev/null */
      nullfd = open("/dev/null", O_RDWR);
      dup2(nullfd, STDOUT_FILENO);
      dup2(nullfd, STDERR_FILENO);
      dup2(nullfd, STDIN_FILENO);
      close(nullfd);
    }
  
  /* if we are to run scripts, we need to fork a helper before dropping root. */
#ifndef NO_FORK
  daemon->helperfd = create_helper(pipewrite, max_fd);
#endif
   
  ent_pw = daemon->username ? getpwnam(daemon->username) : NULL;

  /* before here, we should only call die(), after here, only call syslog() */
  log_start(ent_pw); 

  if (!(daemon->options & OPT_DEBUG))   
    {
      /* UID changing, etc */
      if (daemon->groupname || ent_pw)
	{
	  gid_t dummy;
	  struct group *gp;
	  
	  /* change group for /etc/ppp/resolv.conf otherwise get the group for "nobody" */
	  if ((daemon->groupname && (gp = getgrnam(daemon->groupname))) || 
	      (ent_pw && (gp = getgrgid(ent_pw->pw_gid))))
	    {
	      /* remove all supplimentary groups */
	      setgroups(0, &dummy);
	      setgid(gp->gr_gid);
	    } 
	}
      
      if (ent_pw && ent_pw->pw_uid != 0)
	{     
#ifdef HAVE_LINUX_NETWORK
	  /* On linux, we keep CAP_NETADMIN (for ARP-injection) and
	     CAP_NET_RAW (for icmp) if we're doing dhcp */
	  cap_user_header_t hdr = safe_malloc(sizeof(*hdr));
	  cap_user_data_t data = safe_malloc(sizeof(*data));
	  hdr->version = _LINUX_CAPABILITY_VERSION;
	  hdr->pid = 0; /* this process */
	  data->effective = data->permitted = data->inheritable =
	    (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) |
	    (1 << CAP_SETGID) | (1 << CAP_SETUID);
	  
	  /* Tell kernel to not clear capabilities when dropping root */
	  if (capset(hdr, data) == -1 || prctl(PR_SET_KEEPCAPS, 1) == -1)
	    bad_capabilities = errno;
	  else
#endif
	    {
	      /* finally drop root */
	      setuid(ent_pw->pw_uid);
	      
#ifdef HAVE_LINUX_NETWORK
	      data->effective = data->permitted = 
		(1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW);
	      data->inheritable = 0;
	      
	      /* lose the setuid and setgid capbilities */
	      capset(hdr, data);
#endif
	    }
	}
    }
  
#ifdef HAVE_LINUX_NETWORK
  if (daemon->options & OPT_DEBUG) 
    prctl(PR_SET_DUMPABLE, 1);
#endif

  if (daemon->cachesize != 0)
    my_syslog(LOG_INFO, _("started, version %s cachesize %d"), VERSION, daemon->cachesize);
  else
    my_syslog(LOG_INFO, _("started, version %s cache disabled"), VERSION);
  
  my_syslog(LOG_INFO, _("compile time options: %s"), compile_opts);
  
#ifdef HAVE_DBUS
  if (daemon->options & OPT_DBUS)
    {
      if (daemon->dbus)
	my_syslog(LOG_INFO, _("DBus support enabled: connected to system bus"));
      else
	my_syslog(LOG_INFO, _("DBus support enabled: bus connection pending"));
    }
#endif

  if (bind_fallback)
    my_syslog(LOG_WARNING, _("setting --bind-interfaces option because of OS limitations"));
  
  if (!(daemon->options & OPT_NOWILD)) 
    for (if_tmp = daemon->if_names; if_tmp; if_tmp = if_tmp->next)
      if (if_tmp->name && !if_tmp->used)
	my_syslog(LOG_WARNING, _("warning: interface %s does not currently exist"), if_tmp->name);
   
  if (daemon->options & OPT_NO_RESOLV)
    {
      if (daemon->resolv_files && !daemon->resolv_files->is_default)
	my_syslog(LOG_WARNING, _("warning: ignoring resolv-file flag because no-resolv is set"));
      daemon->resolv_files = NULL;
      if (!daemon->servers)
	my_syslog(LOG_WARNING, _("warning: no upstream servers configured"));
    } 

  if (daemon->max_logs != 0)
    my_syslog(LOG_INFO, _("asynchronous logging enabled, queue limit is %d messages"), daemon->max_logs);

  if (daemon->dhcp)
    {
      struct dhcp_context *dhcp_tmp;
      
      for (dhcp_tmp = daemon->dhcp; dhcp_tmp; dhcp_tmp = dhcp_tmp->next)
	{
	  prettyprint_time(daemon->dhcp_buff2, dhcp_tmp->lease_time);
	  strcpy(daemon->dhcp_buff, inet_ntoa(dhcp_tmp->start));
	  my_syslog(LOG_INFO, 
		    (dhcp_tmp->flags & CONTEXT_STATIC) ? 
		    _("DHCP, static leases only on %.0s%s, lease time %s") :
		    _("DHCP, IP range %s -- %s, lease time %s"),
		    daemon->dhcp_buff, inet_ntoa(dhcp_tmp->end), daemon->dhcp_buff2);
	}
    }

#ifdef HAVE_TFTP
  if (daemon->options & OPT_TFTP)
    {
#ifdef FD_SETSIZE
      if (FD_SETSIZE < (unsigned)max_fd)
	max_fd = FD_SETSIZE;
#endif

      my_syslog(LOG_INFO, "TFTP %s%s %s", 
		daemon->tftp_prefix ? _("root is ") : _("enabled"),
		daemon->tftp_prefix ? daemon->tftp_prefix: "",
		daemon->options & OPT_TFTP_SECURE ? _("secure mode") : "");
      
      /* This is a guess, it assumes that for small limits, 
	 disjoint files might be served, but for large limits, 
	 a single file will be sent to may clients (the file only needs
	 one fd). */

      max_fd -= 30; /* use other than TFTP */
      
      if (max_fd < 0)
	max_fd = 5;
      else if (max_fd < 100)
	max_fd = max_fd/2;
      else
	max_fd = max_fd - 20;

      if (daemon->tftp_max > max_fd)
	{
	  daemon->tftp_max = max_fd;
	  my_syslog(LOG_WARNING, 
		    _("restricting maximum simultaneous TFTP transfers to %d"), 
		    daemon->tftp_max);
	}
    }
#endif

  if (!(daemon->options & OPT_DEBUG) && (getuid() == 0 || geteuid() == 0))
    {
      if (bad_capabilities)
	my_syslog(LOG_WARNING, _("warning: setting capabilities failed: %s"), strerror(bad_capabilities));

      my_syslog(LOG_WARNING, _("running as root"));
    }
  
  check_servers();

  pid = getpid();
  
  while (1)
    {
      int maxfd = -1;
      struct timeval t, *tp = NULL;
      fd_set rset, wset, eset;
      
      FD_ZERO(&rset);
      FD_ZERO(&wset);
      FD_ZERO(&eset);
      
      /* if we are out of resources, find how long we have to wait
	 for some to come free, we'll loop around then and restart
	 listening for queries */
      if ((t.tv_sec = set_dns_listeners(now, &rset, &maxfd)) != 0)
	{
	  t.tv_usec = 0;
	  tp = &t;
	}

      /* Whilst polling for the dbus, or doing a tftp transfer, wake every quarter second */
      if (daemon->tftp_trans ||
	  ((daemon->options & OPT_DBUS) && !daemon->dbus))
	{
	  t.tv_sec = 0;
	  t.tv_usec = 250000;
	  tp = &t;
	}

#ifdef HAVE_DBUS
      set_dbus_listeners(&maxfd, &rset, &wset, &eset);
#endif	
  
      if (daemon->dhcp)
	{
	  FD_SET(daemon->dhcpfd, &rset);
	  bump_maxfd(daemon->dhcpfd, &maxfd);
	}

#ifdef HAVE_LINUX_NETWORK
      FD_SET(daemon->netlinkfd, &rset);
      bump_maxfd(daemon->netlinkfd, &maxfd);
#endif
      
      FD_SET(piperead, &rset);
      bump_maxfd(piperead, &maxfd);

#ifndef NO_FORK
      while (helper_buf_empty() && do_script_run(now));

      if (!helper_buf_empty())
	{
	  FD_SET(daemon->helperfd, &wset);
	  bump_maxfd(daemon->helperfd, &maxfd);
	}
#else
      /* need this for other side-effects */
      while (do_script_run(now));
#endif
      
      /* must do this just before select(), when we know no
	 more calls to my_syslog() can occur */
      set_log_writer(&wset, &maxfd);
      
      if (select(maxfd+1, &rset, &wset, &eset, tp) < 0)
	{
	  /* otherwise undefined after error */
	  FD_ZERO(&rset); FD_ZERO(&wset); FD_ZERO(&eset);
	}

      now = dnsmasq_time();

      check_log_writer(&wset);

      /* Check for changes to resolv files once per second max. */
      /* Don't go silent for long periods if the clock goes backwards. */
      if (last == 0 || difftime(now, last) > 1.0 || difftime(now, last) < -1.0)
	{
	  last = now;

#ifdef HAVE_ISC_READER
	  if (daemon->lease_file && !daemon->dhcp)
	    load_dhcp(now);
#endif

	  if (!(daemon->options & OPT_NO_POLL))
	    poll_resolv();
	}
      
      if (FD_ISSET(piperead, &rset))
	async_event(piperead, now);
      
#ifdef HAVE_LINUX_NETWORK
      if (FD_ISSET(daemon->netlinkfd, &rset))
	netlink_multicast();
#endif
      
#ifdef HAVE_DBUS
      /* if we didn't create a DBus connection, retry now. */ 
     if ((daemon->options & OPT_DBUS) && !daemon->dbus)
	{
	  char *err;
	  if ((err = dbus_init()))
	    my_syslog(LOG_WARNING, _("DBus error: %s"), err);
	  if (daemon->dbus)
	    my_syslog(LOG_INFO, _("connected to system DBus"));
	}
      check_dbus_listeners(&rset, &wset, &eset);
#endif

      check_dns_listeners(&rset, now);

#ifdef HAVE_TFTP
      check_tftp_listeners(&rset, now);
#endif      

      if (daemon->dhcp && FD_ISSET(daemon->dhcpfd, &rset))
	dhcp_packet(now);

#ifndef NO_FORK
      if (daemon->helperfd != -1 && FD_ISSET(daemon->helperfd, &wset))
	helper_write();
#endif

    }
}

static void sig_handler(int sig)
{
  if (pid == 0)
    {
      /* ignore anything other than TERM during startup
	 and in helper proc. (helper ignore TERM too) */
      if (sig == SIGTERM)
	exit(EC_MISC);
    }
  else if (pid != getpid())
    {
      /* alarm is used to kill TCP children after a fixed time. */
      if (sig == SIGALRM)
	_exit(0);
    }
  else
    {
      /* master process */
      int event, errsave = errno;
      
      if (sig == SIGHUP)
	event = EVENT_RELOAD;
      else if (sig == SIGCHLD)
	event = EVENT_CHILD;
      else if (sig == SIGALRM)
	event = EVENT_ALARM;
      else if (sig == SIGTERM)
	event = EVENT_TERM;
      else if (sig == SIGUSR1)
	event = EVENT_DUMP;
      else if (sig == SIGUSR2)
	event = EVENT_REOPEN;
      else
	return;

      send_event(pipewrite, event, 0); 
      errno = errsave;
    }
}

void send_event(int fd, int event, int data)
{
  struct event_desc ev;
  
  ev.event = event;
  ev.data = data;
  /* pipe is non-blocking and struct event_desc is smaller than
     PIPE_BUF, so this either fails or writes everything */
  while (write(fd, &ev, sizeof(ev)) == -1 && errno == EINTR);
}

static void async_event(int pipe, time_t now)
{
  pid_t p;
  struct event_desc ev;
  int i;

  if (read_write(pipe, (unsigned char *)&ev, sizeof(ev), 1))
    switch (ev.event)
      {
      case EVENT_RELOAD:
	clear_cache_and_reload(now);
	if (daemon->resolv_files && (daemon->options & OPT_NO_POLL))
	  {
	    reload_servers(daemon->resolv_files->name);
	    check_servers();
	  }
	rerun_scripts();
	break;
	
      case EVENT_DUMP:
	dump_cache(now);
	break;
	
      case EVENT_ALARM:
	if (daemon->dhcp)
	  {
	    lease_prune(NULL, now);
	    lease_update_file(now);
	  }
	break;
		
      case EVENT_CHILD:
	/* See Stevens 5.10 */
	while ((p = waitpid(-1, NULL, WNOHANG)) != 0)
	  if (p == -1)
	    {
	      if (errno != EINTR)
		break;
	    }      
	  else 
	    for (i = 0 ; i < MAX_PROCS; i++)
	      if (daemon->tcp_pids[i] == p)
		daemon->tcp_pids[i] = 0;
	break;
	
      case EVENT_KILLED:
	my_syslog(LOG_WARNING, _("child process killed by signal %d"), ev.data);
	break;

      case EVENT_EXITED:
	my_syslog(LOG_WARNING, _("child process exited with status %d"), ev.data);
	break;

      case EVENT_EXEC_ERR:
	my_syslog(LOG_ERR, _("failed to execute %s: %s"), daemon->lease_change_command, strerror(ev.data));
	break;

      case EVENT_PIPE_ERR:
	my_syslog(LOG_ERR, _("failed to create helper: %s"), strerror(ev.data));
	break;

      case EVENT_REOPEN:
	/* Note: this may leave TCP-handling processes with the old file still open.
	   Since any such process will die in CHILD_LIFETIME or probably much sooner,
	   we leave them logging to the old file. */
	if (daemon->log_file != NULL)
	  log_reopen(daemon->log_file);
	break;
	
      case EVENT_TERM:
	/* Knock all our children on the head. */
	for (i = 0; i < MAX_PROCS; i++)
	  if (daemon->tcp_pids[i] != 0)
	    kill(daemon->tcp_pids[i], SIGALRM);
	
#ifndef NO_FORK
	/* handle pending lease transitions */
	if (daemon->helperfd != -1)
	  {
	    /* block in writes until all done */
	    if ((i = fcntl(daemon->helperfd, F_GETFL)) != -1)
	      fcntl(daemon->helperfd, F_SETFL, i & ~O_NONBLOCK); 
	    do {
	      helper_write();
	    } while (!helper_buf_empty() || do_script_run(now));
	    close(daemon->helperfd);
	  }
#endif
	
	if (daemon->lease_stream)
	  fclose(daemon->lease_stream);
	
	my_syslog(LOG_INFO, _("exiting on receipt of SIGTERM"));
	flush_log();
	exit(EC_GOOD);
      }
}

static void poll_resolv()
{
  struct resolvc *res, *latest;
  struct stat statbuf;
  time_t last_change = 0;
  /* There may be more than one possible file. 
     Go through and find the one which changed _last_.
     Warn of any which can't be read. */
  for (latest = NULL, res = daemon->resolv_files; res; res = res->next)
    if (stat(res->name, &statbuf) == -1)
      {
	if (!res->logged)
	  my_syslog(LOG_WARNING, _("failed to access %s: %s"), res->name, strerror(errno));
	res->logged = 1;
      }
    else
      {
	res->logged = 0;
	if (statbuf.st_mtime != res->mtime)
	  {
	    res->mtime = statbuf.st_mtime;
	    if (difftime(statbuf.st_mtime, last_change) > 0.0)
	      {
		last_change = statbuf.st_mtime;
		latest = res;
	      }
	  }
      }
  
  if (latest)
    {
      static int warned = 0;
      if (reload_servers(latest->name))
	{
	  my_syslog(LOG_INFO, _("reading %s"), latest->name);
	  warned = 0;
	  check_servers();
	  if (daemon->options & OPT_RELOAD)
	    cache_reload(daemon->options, daemon->namebuff, daemon->domain_suffix, daemon->addn_hosts);
	}
      else 
	{
	  latest->mtime = 0;
	  if (!warned)
	    {
	      my_syslog(LOG_WARNING, _("no servers found in %s, will retry"), latest->name);
	      warned = 1;
	    }
	}
    }
}       

void clear_cache_and_reload(time_t now)
{
  cache_reload(daemon->options, daemon->namebuff, daemon->domain_suffix, daemon->addn_hosts);
  if (daemon->dhcp)
    {
      if (daemon->options & OPT_ETHERS)
	dhcp_read_ethers();
      if (daemon->dhcp_hosts_file)
	dhcp_read_hosts();
      dhcp_update_configs(daemon->dhcp_conf);
      lease_update_from_configs(); 
      lease_update_file(now); 
      lease_update_dns();
    }
}

static int set_dns_listeners(time_t now, fd_set *set, int *maxfdp)
{
  struct serverfd *serverfdp;
  struct listener *listener;
  int wait, i;
  
#ifdef HAVE_TFTP
  int  tftp = 0;
  struct tftp_transfer *transfer;
  for (transfer = daemon->tftp_trans; transfer; transfer = transfer->next)
    {
      tftp++;
      FD_SET(transfer->sockfd, set);
      bump_maxfd(transfer->sockfd, maxfdp);
    }
#endif
  
  /* will we be able to get memory? */
  get_new_frec(now, &wait);
  
  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
    {
      FD_SET(serverfdp->fd, set);
      bump_maxfd(serverfdp->fd, maxfdp);
    }
	  
  for (listener = daemon->listeners; listener; listener = listener->next)
    {
      /* only listen for queries if we have resources */
      if (wait == 0)
	{
	  FD_SET(listener->fd, set);
	  bump_maxfd(listener->fd, maxfdp);
	}

      /* death of a child goes through the select loop, so
	 we don't need to explicitly arrange to wake up here */
      for (i = 0; i < MAX_PROCS; i++)
	if (daemon->tcp_pids[i] == 0)
	  {
	    FD_SET(listener->tcpfd, set);
	    bump_maxfd(listener->tcpfd, maxfdp);
	    break;
	  }

#ifdef HAVE_TFTP
      if (tftp <= daemon->tftp_max && listener->tftpfd != -1)
	{
	  FD_SET(listener->tftpfd, set);
	  bump_maxfd(listener->tftpfd, maxfdp);
	}
#endif

    }
  
  return wait;
}

static void check_dns_listeners(fd_set *set, time_t now)
{
  struct serverfd *serverfdp;
  struct listener *listener;	  
  
  for (serverfdp = daemon->sfds; serverfdp; serverfdp = serverfdp->next)
    if (FD_ISSET(serverfdp->fd, set))
      reply_query(serverfdp, now);
  
  for (listener = daemon->listeners; listener; listener = listener->next)
    {
      if (FD_ISSET(listener->fd, set))
	receive_query(listener, now); 
 
#ifdef HAVE_TFTP     
      if (listener->tftpfd != -1 && FD_ISSET(listener->tftpfd, set))
	tftp_request(listener, now);
#endif

      if (FD_ISSET(listener->tcpfd, set))
	{
	  int confd;
	  struct irec *iface = NULL;
	  pid_t p;
	  
	  while((confd = accept(listener->tcpfd, NULL, NULL)) == -1 && errno == EINTR);
	  
	  if (confd == -1)
	    continue;
	  
	  if (daemon->options & OPT_NOWILD)
	    iface = listener->iface;
	  else
	    {
	      union mysockaddr tcp_addr;
	      socklen_t tcp_len = sizeof(union mysockaddr);
	      /* Check for allowed interfaces when binding the wildcard address:
		 we do this by looking for an interface with the same address as 
		 the local address of the TCP connection, then looking to see if that's
		 an allowed interface. As a side effect, we get the netmask of the
		 interface too, for localisation. */
	      
	      /* interface may be new since startup */
	      if (enumerate_interfaces() &&
		  getsockname(confd, (struct sockaddr *)&tcp_addr, &tcp_len) != -1)
		for (iface = daemon->interfaces; iface; iface = iface->next)
		  if (sockaddr_isequal(&iface->addr, &tcp_addr))
		    break;
	    }
	  
	  if (!iface)
	    {
	      shutdown(confd, SHUT_RDWR);
	      close(confd);
	    }
#ifndef NO_FORK
	  else if (!(daemon->options & OPT_DEBUG) && (p = fork()) != 0)
	    {
	      if (p != -1)
		{
		  int i;
		  for (i = 0; i < MAX_PROCS; i++)
		    if (daemon->tcp_pids[i] == 0)
		      {
			daemon->tcp_pids[i] = p;
			break;
		      }
		}
	      close(confd);
	    }
#endif
	  else
	    {
	      unsigned char *buff;
	      struct server *s; 
	      int flags;
	      struct in_addr dst_addr_4;
	      
	      dst_addr_4.s_addr = 0;
	      
	       /* Arrange for SIGALARM after CHILD_LIFETIME seconds to
		  terminate the process. */
	      if (!(daemon->options & OPT_DEBUG))
		alarm(CHILD_LIFETIME);
	      
	      /* start with no upstream connections. */
	      for (s = daemon->servers; s; s = s->next)
		 s->tcpfd = -1; 
	      
	      /* The connected socket inherits non-blocking
		 attribute from the listening socket. 
		 Reset that here. */
	      if ((flags = fcntl(confd, F_GETFL, 0)) != -1)
		fcntl(confd, F_SETFL, flags & ~O_NONBLOCK);
	      
	      if (listener->family == AF_INET)
		dst_addr_4 = iface->addr.in.sin_addr;
	      
	      buff = tcp_request(confd, now, dst_addr_4, iface->netmask);
	       
	      shutdown(confd, SHUT_RDWR);
	      close(confd);
	      
	      if (buff)
		free(buff);
	      
	      for (s = daemon->servers; s; s = s->next)
		if (s->tcpfd != -1)
		  {
		    shutdown(s->tcpfd, SHUT_RDWR);
		    close(s->tcpfd);
		  }
#ifndef NO_FORK		   
	      if (!(daemon->options & OPT_DEBUG))
		{
		  flush_log();
		  _exit(0);
		}
#endif
	    }
	}
    }
}


int make_icmp_sock(void)
{
  int fd;
  int zeroopt = 0;

  if ((fd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) != -1)
    {
      if (!fix_fd(fd) ||
	  setsockopt(fd, SOL_SOCKET, SO_DONTROUTE, &zeroopt, sizeof(zeroopt)) == -1)
	{
	  close(fd);
	  fd = -1;
	}
    }

  return fd;
}

int icmp_ping(struct in_addr addr)
{
  /* Try and get an ICMP echo from a machine. */

  /* Note that whilst in the three second wait, we check for 
     (and service) events on the DNS and TFTP  sockets, (so doing that
     better not use any resources our caller has in use...)
     but we remain deaf to signals or further DHCP packets. */

  int fd;
  struct sockaddr_in saddr;
  struct { 
    struct ip ip;
    struct icmp icmp;
  } packet;
  unsigned short id = rand16();
  unsigned int i, j;
  int gotreply = 0;
  time_t start, now;

#ifdef HAVE_LINUX_NETWORK
  if ((fd = make_icmp_sock()) == -1)
    return 0;
#else
  int opt = 2000;
  fd = daemon->dhcp_icmp_fd;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
#endif

  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr = addr;
#ifdef HAVE_SOCKADDR_SA_LEN
  saddr.sin_len = sizeof(struct sockaddr_in);
#endif
  
  memset(&packet.icmp, 0, sizeof(packet.icmp));
  packet.icmp.icmp_type = ICMP_ECHO;
  packet.icmp.icmp_id = id;
  for (j = 0, i = 0; i < sizeof(struct icmp) / 2; i++)
    j += ((u16 *)&packet.icmp)[i];
  while (j>>16)
    j = (j & 0xffff) + (j >> 16);  
  packet.icmp.icmp_cksum = (j == 0xffff) ? j : ~j;
  
  while (sendto(fd, (char *)&packet.icmp, sizeof(struct icmp), 0, 
		(struct sockaddr *)&saddr, sizeof(saddr)) == -1 &&
	 retry_send());
  
  for (now = start = dnsmasq_time(); 
       difftime(now, start) < (float)PING_WAIT;)
    {
      struct timeval tv;
      fd_set rset, wset;
      struct sockaddr_in faddr;
      int maxfd = fd; 
      socklen_t len = sizeof(faddr);
      
      tv.tv_usec = 250000;
      tv.tv_sec = 0; 
      
      FD_ZERO(&rset);
      FD_ZERO(&wset);
      FD_SET(fd, &rset);
      set_dns_listeners(now, &rset, &maxfd);
      set_log_writer(&wset, &maxfd);

      if (select(maxfd+1, &rset, &wset, NULL, &tv) < 0)
	{
	  FD_ZERO(&rset);
	  FD_ZERO(&wset);
	}

      now = dnsmasq_time();

      check_log_writer(&wset);
      check_dns_listeners(&rset, now);

#ifdef HAVE_TFTP
      check_tftp_listeners(&rset, now);
#endif

      if (FD_ISSET(fd, &rset) &&
	  recvfrom(fd, &packet, sizeof(packet), 0,
		   (struct sockaddr *)&faddr, &len) == sizeof(packet) &&
	  saddr.sin_addr.s_addr == faddr.sin_addr.s_addr &&
	  packet.icmp.icmp_type == ICMP_ECHOREPLY &&
	  packet.icmp.icmp_seq == 0 &&
	  packet.icmp.icmp_id == id)
	{
	  gotreply = 1;
	  break;
	}
    }
  
#ifdef HAVE_LINUX_NETWORK
  close(fd);
#else
  opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
#endif

  return gotreply;
}

 