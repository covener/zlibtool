/*
 * $Log$
 * Revision 1.1  2000/06/29 15:26:59  trawick
 * initial check-in
 *
 */

static const char rcsid[] = "$Id$";

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>

#define PGM "libtoolexe"

#define MAX_LINE 1024

#define MAX_CMDS 100

struct CmdRec
{
  const char *target;
  const char *cmd;
};

static int numCmds;
static struct CmdRec cmds[MAX_CMDS];
static int debug;

static void addCmd(const char *target,const char *cmd)
{
  if (numCmds == MAX_CMDS)
  {
    fprintf(stderr,
            PGM ": At most %d commands can be configured.\n",
            MAX_CMDS);
    exit(1);
  }

  cmds[numCmds].target = strdup(target);
  cmds[numCmds].cmd = strdup(cmd);
  ++numCmds;
}

static int runCmds(const char *target)
{
  int curCmd, rc;

  if (debug)
  {
    printf(PGM ": Looking for commands for target `%s'\n",target);
  }

  curCmd = 0;
  rc = 0;

  while (!rc && curCmd < numCmds)
  {
    if (!strcmp(target,cmds[curCmd].target))
    {
      int orc;

      printf(PGM ": %s\n",cmds[curCmd].cmd);

      orc = system(cmds[curCmd].cmd);
      if (WIFEXITED(orc))
      {
        rc = WEXITSTATUS(orc);
      }
      else
        rc = 1;
    }
    ++curCmd;
  }

  return rc;
}

static int readCfgFile(void)
{
  const char *fname = ".libtoolconf";
  FILE *cfg;
  char line[MAX_LINE + 1];
  char *ch, *colon, *newline, *target, *cmd;
  int rc = 0;
  int curLine = 0;

  cfg = fopen(fname,"r");
  if (cfg)                         /* If we found a config file... */
  {
    do
    {
      ch = fgets(line,sizeof line,cfg);
      if (ch)
      {
        ++curLine;
        newline = strchr(line,'\n');
        if (!newline)
        {
          fprintf(stderr,
                  PGM ": Warning: line %d in %s is too long.\n",
                  curLine,fname);
        }
        else
        {
          *newline = '\0';
        }
        if (debug)
        {
          printf(PGM ": I just read `%s'\n",line);
        }
        colon = strchr(line,':');
        if (!colon)
        {
          fprintf(stderr,
                  PGM ": Error: bad syntax on line %d.\n",
                  curLine);
          rc = 1;
        }
        else
        {
          *colon = '\0';
          target = line;
          cmd = colon + 1;
          addCmd(target,cmd);
        }
      }
    } while (!rc && ch);

    if (ferror(cfg))
    {
      fprintf(stderr,
              PGM ": An error occurred reading from the cfg file: %s\n",
              strerror(errno));
      rc = 1;
    }
    fclose(cfg);
  }
  return rc;
}

int main(int argc,char **argv)
{
  int curArg, rc, orc;
  char cmdline[40000];

  debug = getenv("LIBTOOLDEBUG") != NULL;

  if (argc == 1)
  {
    fprintf(stderr,
            PGM ": Please run Jeff's libtool with some parameters!\n");
    exit(1);
  }

  rc = readCfgFile();
  if (rc)
  {
    fprintf(stderr,PGM ": readCfgFile()->%d\n",rc);
    exit(1);
  }
  
  curArg = 1;
  while (curArg < argc)
  {
    if (!strcmp(argv[curArg],"--version"))
    {
      /*
       * Note: We don't print "OS/390" in the following message because
       *       that throws off Apache's sed code to strip the version
       *       number out of the output.
       */

      printf(PGM ": This is libtool 1.3.4 for OS/three-ninety.\n"
 	     "It acts enough like GNU libtool to allow Apache to be built.\n");
      exit(0);
    }

    /*
     * Hack: also look for c89, because mm makefiles are
     * using "libtool c89" for Greg.
     */
    
    if (!strcmp(argv[curArg],"cc") ||
        !strcmp(argv[curArg],"c89"))
    {
      enum {NORM=1,TARGET} state = NORM;
      char *ccpos = cmdline;
      char *dashopos = NULL;
      const char *target;
      
      /*
       * This is the start of the command to be executed.
       */

      cmdline[0] = '\0';
      while (curArg < argc)
      {
        size_t len;

        if (!strcmp(argv[curArg],"-rpath") ||
            !strcmp(argv[curArg],"-version-info"))
        {
          /*
           * -rpath ARG and -version-info ARG aren't supported.
           *
           * Skip it.
           */

          if (! (curArg + 1 < argc))
          {
            fprintf(stderr,
                    PGM ": I'm confused... It looks like `%s' doesn't have "
                    "an argument.\n",
                    argv[curArg]);
            exit(1);
          }

          if (debug)
              printf(PGM ": Ignoring \"%s %s\"...\n",
                     argv[curArg], argv[curArg + 1]);
          
          curArg += 2;
          continue;                 /* Just pretend -rpath ARG wasn't
                                       on the command-line.          */
        }

        if (!strcmp(argv[curArg],"-export-dynamic"))
        {
          /*
           * -export-dynamic isn't supported... skip it
           */

          ++curArg;
          continue;
        }
        
        strcat(cmdline,argv[curArg]);

        /*
         * If this is cc or -o, leave extra room because the commands/
         * options we overlay them with may be larger.
         *
         * Hack: also look for c89, because mm makefiles are
         * using "libtool c89" for Greg.
         */
        
        if (!strcmp(argv[curArg],"cc") ||
            !strcmp(argv[curArg],"c89") ||
            !strcmp(argv[curArg],"-o"))
          strcat(cmdline,"  ");
            
        len = strlen(argv[curArg]);

        /* turn foo.lo into foo.o */
        if (argv[curArg][len - 3] == '.' &&
            argv[curArg][len - 2] == 'l' &&
            argv[curArg][len - 1] == 'o')
        {
          len = strlen(cmdline);
          assert(cmdline[len - 3] == '.');
          assert(cmdline[len - 2] == 'l');
          cmdline[len - 2] = 'o';
          cmdline[len - 1] = '\0';
        }
        strcat(cmdline," ");

        if (state == TARGET)
        {
          state = NORM;
          target = argv[curArg];
          
          if (argv[curArg][len - 3] == '.' &&
              argv[curArg][len - 2] == 'l' &&
              argv[curArg][len - 1] == 'a')
          {
            char *ch;
            
            /*
             * Hack: also look for c89, because mm makefiles are
             * using "libtool c89" for Greg.
             */

            assert(!memcmp(ccpos,"cc ",3) ||
                   !memcmp(ccpos,"c89 ",4));
            assert(dashopos);
            assert(dashopos > ccpos);
            ch = ccpos;
            while (ch < dashopos)
            {
              *ch = ' ';
              ++ch;
            }
            memcpy(ccpos,"ar  ",4); /* overlay cc command            */
            memcpy(dashopos,"-rs ",4); /* overlay -o argument        */
            ccpos = NULL;
            dashopos = NULL;
          }
        }
        else if (state == NORM &&
                 !strcmp(argv[curArg],"-o"))
        {
          state = TARGET;
          dashopos = cmdline + strlen(cmdline) - strlen("-o   ");
        }
        
        ++curArg;
      }
      assert(state == NORM);
      printf(PGM ": %s\n",cmdline);
      fflush(stdout);
      orc = system(cmdline);
      if (WIFEXITED(orc))
      {
        rc = WEXITSTATUS(orc);
      }
      else
        rc = 1;
      if (!rc)
        rc = runCmds(target);
      if (rc)
        fprintf(stderr,PGM ": returning error code %d (%X)...\n",rc,orc);
      exit(rc);
    }
    
    ++curArg;
  }

  return 0;
}
