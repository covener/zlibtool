/*
 * $Log$
 */

static const char rcsid[] = "$Id$";

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/wait.h>

int main(int argc,char **argv)
{
  int debug = getenv("LIBTOOLDEBUG") != NULL;
  int curArg, rc, orc;
  char cmdline[40000];

  if (argc == 1)
  {
    fprintf(stderr,
            "Please run Jeff's libtool with some parameters!\n");
    exit(1);
  }
  
  curArg = 1;
  while (curArg < argc)
  {
    if (!strcmp(argv[curArg],"--version"))
    {
      printf("This is not ltmain.sh (GNU libtool) 1.3.4 (1.385.2.196 1999/12/07 21:47:57)\n"
             "Instead, it is libtool for OS/390, which is just enough of "
             "libtool to build apache 2.0 and above.\n");
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
                    "I'm confused... It looks like `%s' doesn't have "
                    "an argument.\n",
                    argv[curArg]);
            exit(1);
          }

          if (debug)
              printf("Ignoring \"%s %s\"...\n",
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
      printf("%s\n",cmdline);
      fflush(stdout);
      orc = system(cmdline);
      if (WIFEXITED(orc))
      {
        rc = WEXITSTATUS(orc);
      }
      else
        rc = 1;
      if (rc)
        fprintf(stderr,"returning error code %d (%X)...\n",rc,orc);
      exit(rc);
    }
    
    ++curArg;
  }

  return 0;
}
