/*
 * $Log$
 * Revision 1.18  2001/01/04 20:10:57  trawick
 * fix references to http_main in the code which handles a DLL build
 *
 * Revision 1.17  2001/01/04 19:51:35  trawick
 * Fix the hack which adds runopts(STACK(,,ANY)) to the file with main().
 * The name of that file changed from http_main.c to main.c.
 *
 * Note that this generic type of name could cause problems when this
 * libtool is used with other projects.
 *
 * Revision 1.16  2000/12/28 22:01:29  trawick
 * ignore EEXIST errors from symlink()
 *
 * Revision 1.15  2000/12/22 21:59:45  gregames
 *
 * prevent possible storage overlay
 *
 * Revision 1.14  2000/12/22 20:34:22  gregames
 * buildArchive - fix error in the previous patch.  If multiple archives are created
 * in the same directory, we need to handle error from mkdir(".libs")
 *
 * Revision 1.13  2000/12/22 19:52:49  gregames
 *
 * buildArchive - create a .libs directory containing a symlink to ../foo.a after
 * creating the archive
 *
 * Revision 1.12  2000/11/02 22:28:39  trawick
 * Handle compile options mixed in with the input files.
 *
 * TODO: Give every argument an input type.
 *
 * Revision 1.11  2000/10/31 22:02:52  trawick
 * As we build command lines via addArg(), escape any shell metacharacters.
 * This fixes a nasty bug found by Ovies Brabson.
 *
 * If we don't escape shell metacharacters, the shell will try to interpret
 * them.  But this is a compiler command line, and parentheses and other such
 * chars should be passed to the compiler.
 *
 * Putting something like "-Wc,LANGLVL(EXTENDED)" on the libtool command-line
 * now works.
 *
 * Revision 1.10  2000/08/31 15:04:26  trawick
 * Add back support for editInputFile() processing.  It was lost during the
 * big rewrite of command-line parsing.
 *
 * Teach the command-line parser to identify .c files on a compile as input
 * files.
 *
 * Fix a couple of messages to use the PGM prefix for identifying the source
 * of the message.
 *
 * Revision 1.9  2000/08/30 15:29:14  trawick
 * Add support for LIBTOOL_CFLAGS and LIBTOOL_LFLAGS.
 *
 * Revision 1.8  2000/08/22 21:13:24  trawick
 * fix buildingDll check; before this, it thought we were always building
 * a dll because of the bogus !strstr() logic
 *
 * Revision 1.7  2000/08/18 20:53:28  trawick
 * Fix snafu in previous commit.
 *
 * Revision 1.6  2000/08/18 14:27:13  trawick
 * Use "apachecore.dll" instead of "httpdcore.dll" to be more consistent with
 * Win32.
 *
 * Revision 1.5  2000/08/15 17:25:49  trawick
 * Fix some bugs where ignored input files (e.g., *.h) were not
 * really ignored, and a NULL .realInput field was accessed.
 *
 * Revision 1.4  2000/08/14 14:59:47  trawick
 * Add initial support for building Apache 2.0 dsos.
 *
 * Known problems with this level of code:
 *
 * 1) libtoolexe.c code needs to be split up; too darn big
 * 2) dsos aren't linked until "make install", which is too late
 *
 * Revision 1.3  2000/07/05 16:55:33  trawick
 * Add initial (hokey) support for adding text to the top of a source
 * file.  Currently, this is hard-coded to add a pragma runopt to
 * http_main.c.
 *
 * Revision 1.2  2000/06/30 13:38:38  trawick
 * Add ability to specify per-target commands in .libtoolconf.
 * These commands are issued just after a successful build of
 * the specified target.
 *
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
#include <sys/stat.h>

#define OS390_BUILD      1

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
#define DEBUG_GORY_DETAILS 3
#define DEBUG_OVERVIEW     2
#define DEBUG_SHOWCMD      1
FILE *debugf;

#define MAX_ARGS 200

#define CORE_BASENAME     "apachecore"
#define CORE_DLL          CORE_BASENAME ".dll"
#define CORE_X            CORE_BASENAME ".x"

typedef struct
{
  const char *s; 
  /* the following fields are always set if this is an input file */
  const char *realInput;
  enum {INPUT_IS_OBJ = 100, INPUT_IS_LOBJ, INPUT_IS_ARCHIVE,
        INPUT_IS_LARCHIVE, INPUT_IS_SOURCE, INPUT_IS_IGNORED, NOT_INPUT,
        INPUT_IS_OPTION} inputType;
} Arg_t;

typedef struct Parms_t
{
  unsigned int fromShlibtool : 1;
  unsigned int silent : 1;
  unsigned int exportDynamic : 1;
  unsigned int module : 1;
  unsigned int avoidVersion : 1;
  unsigned int showVersion : 1;
  unsigned int buildingDll : 1;
  const char *target;
  const char *rpath;
  const char *version;
  enum {COMPILE=3, LINK, SHOWVERSION, INSTALL, UNKNOWN_MODE} mode;
  enum {OUTPUT_IS_OBJ=9, OUTPUT_IS_ARCHIVE, OUTPUT_IS_EXE, UNKNOWN_OUTPUT} outputType;
  Arg_t args[MAX_ARGS];
  int numArgs;
  int firstInput;
} Parms_t;

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

      if (debug >= DEBUG_SHOWCMD)
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

static int insertLine(const char *fname,const char *text)
{
  int rc = 0, done = 0;
  FILE *new = NULL, *old = NULL;
  const char *tmpname = "libtool.tmp";

  if (!rc)
  {
    new = fopen(tmpname,"w");
    if (!new)
    {
      fprintf(stderr,"couldn't create %s: %s\n",tmpname,strerror(errno));
      rc = 1;
    }
  }

  if (!rc)
  {
    old = fopen(fname,"r");
    if (!old)
    {
      fprintf(stderr,"couldn't open %s: %s\n",fname,strerror(errno));
      rc = 1;
    }
  }

  if (!rc)
  {
    char inbuf[1024];

    if (fgets(inbuf,sizeof inbuf,old))
    {
      if (strstr(inbuf,text))
      {
        if (debug)
          printf("already patched...\n");
        done = 1;
      }
      else
        rewind(old);
    }
  }

  if (!rc && !done)
  {
    fprintf(new,"%s\n",text);
    /* fprintf(new,"#line 1 \"%s\"\n",fname); */
    while (!feof(old) && !ferror(old) && !ferror(new))
    {
      char inbuf[1024];
      const char *inline = fgets(inbuf,sizeof inbuf,old);

      if (inline)
      {
        fprintf(new,"%s",inbuf);
      }
    }    
    if (ferror(old) || ferror(new))
    {
      fprintf(stderr,"Disk I/O error: %s\n",strerror(errno));
      rc = 1;
    }
  }

  if (new)
    fclose(new);

  if (old)
    fclose(old);

  if (!rc && !done)
  {
    rc = unlink(fname);
    if (rc)
    {
      rc = 1;
      fprintf(stderr,"remove %s: %s\n",fname,strerror(errno));
    }
  }

  if (!rc && !done)
  {
    rc = rename(tmpname,fname); 
    if (rc)
    {
      rc = 1;
      fprintf(stderr,"rename %s to %s: %s\n",tmpname,fname,strerror(errno));
    }
  }

  return rc;
}

static int editInputFile(const char *inputFile)
{
  int rc = 0;

  if (debug)
    printf("editInputFile(%s)\n",inputFile);

#if OS390_BUILD
  /* XXX hack away! */
  if (!strcmp(inputFile,"main.c"))
  {
    rc = insertLine(inputFile,"#pragma runopts(STACK(,,ANY))");
  }
#endif

  return rc;
}

static int shlibtoolLink(Parms_t *p)
{
  int rc = 0;
  FILE *la;
  char *intendedSo = strdup(p->target);
  char *dotla;

  dotla = strstr(intendedSo,".la");
  assert(dotla);
  strcpy(dotla,".so");

  /*
   * Create the output file
   */

  la = fopen(p->target,"w");
  if (!la)
  {
    fprintf(stderr,"couldn't create %s: %s\n",p->target,strerror(errno));
    exit(1);
  }

  fprintf(la,
	  "# We can't build a dso from %s until install time because\n"
          "# we don't have the .x file from building core.dll\n");
  fprintf(la,
          "# intended shared object: %s\n",intendedSo);
  assert(p->args[p->firstInput].realInput);
  fprintf(la,"input:%s\n",p->args[p->firstInput].realInput);
  fclose(la);

  return rc;
}

static const char *modeStr(int mode)
{
  switch(mode)
  {
    case COMPILE:
      return "COMPILE";
    case LINK: 
      return "LINK";
    case SHOWVERSION:
      return "SHOWVERSION";
    case INSTALL:
      return "INSTALL";
  }
  return "(unknown)";
}

static const char *inputTypeStr(int type)
{
  switch(type)
  {
    case INPUT_IS_OBJ:
      return "OBJ";
    case INPUT_IS_LOBJ:
      return "LOBJ";
    case INPUT_IS_ARCHIVE:
      return "ARCHIVE";
    case INPUT_IS_LARCHIVE:
      return "LARCHIVE";
    case INPUT_IS_SOURCE:
      return "SOURCE";
    case INPUT_IS_IGNORED:
      return "(ignored)";
    case INPUT_IS_OPTION:
      return "(compiler option)";
  }
  return "(unknown)";
}

static const char *outputTypeStr(int type)
{
  switch(type)
  {
    case OUTPUT_IS_OBJ:
      return "OBJECT";
    case OUTPUT_IS_ARCHIVE:
      return "ARCHIVE";
    case OUTPUT_IS_EXE:
      return "EXE";
  }
  return "(unknown)";
}

static void dumpParms(Parms_t *p)
{
  int curArg;

  printf("Mode: %s\n",modeStr(p->mode));
  printf("Output type: %s\n",outputTypeStr(p->outputType));
  printf("Target: %s\n",p->target ? p->target : "(unknown)");
  printf("Flags: %s%s%s%s%s%s\n",
         p->buildingDll   ? "dll "            : "",
         p->fromShlibtool ? "shlibtool "      : "",
         p->silent        ? "silent "         : "",
         p->exportDynamic ? "export-dynamic " : "",
         p->module        ? "module "         : "",
         p->showVersion   ? "show-version "   : "",
         p->avoidVersion  ? "avoid-version "  : "");
  printf("Compiler arguments:\n");
  curArg = 0; 
  while (curArg < p->firstInput)
  {
    printf("\t%s\n",p->args[curArg].s);
    ++curArg;
  }
  printf("Input files:\n");
  while (curArg < p->numArgs)
  {
    printf("\t%-10s %-40s %s\n",
           inputTypeStr(p->args[curArg].inputType),
           p->args[curArg].s,
           p->args[curArg].realInput ? p->args[curArg].realInput : "N/A");
    ++curArg;
  }
}

static int removeFile(const char *f,int fatal)
{
  int rc = 0;
  struct stat sb;

  if (stat(f,&sb) == 0)
  {
    if (unlink(f))
    {
      perror(f);
      exit(1);
    }
  }

  return 0;
}

#define max(x,y) ((x) >= (y) ? (x) : (y))

/* escapeArg(): 
 *
 * Escape any shell metacharacters as we build the 
 * command-line.
 *
 * Shell metacharacters are sometimes used in compiler
 * arguments (e.g., parentheses in "-Wl,LANGLVL(ANSI)")
 * and need to be escaped so that the shell doesn't try  
 * to evaluate them
 */
static const char *escapeArg(const char *add)
{
  int badch = 0; /* did we find a char to escape? */
  char tmparg[1024];
  char *newch = tmparg;
  const char *oldch = add;

  assert(strlen(add) * 2 < sizeof(tmparg));

  while (*oldch)
  {
    switch(*oldch)
    {
      /* check for any characters to escape here! */
      case '(':
      case ')':
      case '~':
      case '#':
      case '^':
      case '&':
      case '*':
      case '{':
      case '}':
      case '[':
      case ']':
        *newch = '\\';
        ++newch;
        badch = 1;
        break;
    }
    *newch = *oldch;
    ++newch;
    ++oldch;
  } 
  *newch = '\0';
  if (badch) /* at least one metachar, so return our new string
              * (but not the copy in autodata :) )
              */
    return strdup(tmparg);
  else
    return add;
}

typedef struct
{
  char *s;
  size_t curLen;
  size_t curSize;
} Cmdline_t;

static void addArg(Cmdline_t *c,const char *add)
{
  size_t addLen;

  add = escapeArg(add);

  assert(add);
  addLen = strlen(add);

  if (addLen == 0)
    return;

  if (c->curLen + addLen + 1 > c->curSize)
  {
    size_t newSize;

    newSize = c->curSize + max(addLen,1024);
    c->s = realloc(c->s,newSize);
    assert(c->s);
    c->curSize = newSize;
  }
  strcpy(c->s + c->curLen,add);
  c->curLen += addLen; 
}

static int runCmd(Parms_t *p,Cmdline_t *c)
{
  int rc = 0, orc;

  if (!rc)
  {
    if (!p->silent || debug >= DEBUG_SHOWCMD)
      printf(PGM ": %s\n",c->s);
    fflush(stdout);
    orc = system(c->s);
    if (WIFEXITED(orc))
    {
      rc = WEXITSTATUS(orc);
    }
  }

  return rc;
}

#define MAX_INPUTS 1000

typedef struct
{
  char *fname;
  int numInputs;
  char *inputs[MAX_INPUTS];
} Larchive_t;

static void dumpLarchive(Larchive_t *la)
{
  int cur;

  fprintf(debugf,"larchive %s:\n",la->fname);
  cur = 0;
  while (cur < la->numInputs)
  {
    fprintf(debugf,"\t%s\n",la->inputs[cur]);
    ++cur;
  }
}

static void loadLarchive(Larchive_t *la,const char *fname)
{
  FILE *in;
  char *inline = malloc(100000);
  char *ch, *tmpch;

  memset(la,0,sizeof(*la));

  la->fname = strdup(fname);
  in = fopen(fname,"r");
  if (!in)
  {
    perror(fname);
    exit(1);
  }
  while (!ferror(in) && !feof(in))
  {
    ch = fgets(inline,100000,in);
    if (ch)
    {
      if (inline[strlen(inline) - 1] != '\n')
      {
        fprintf(stderr,"line too big in %s\n",fname);
        exit(1);
      }
      inline[strlen(inline) - 1] = '\0';
      if (inline[0] == '#')
        continue;
      if (!memcmp(inline,"input:",6))
      {
        ch = inline + 6;
        while (*ch)
        {
          if (isspace(*ch))
            ++ch;
          else
          {
            la->inputs[la->numInputs] = ch;
            ++la->numInputs;
            tmpch = strchr(ch,' ');
            if (tmpch)
            {
              *tmpch = '\0';
              ch = tmpch + 1;
            }
            else
              ch = ch + strlen(ch);
            /* Now, make a copy... */
            la->inputs[la->numInputs - 1] =
              strdup(la->inputs[la->numInputs - 1]);
          }
        }
        continue;
      }
      fprintf(stderr,
              "syntax error in %s: %s\n",
              fname,inline);
    }
  }

  fclose(in);

  free(inline); 
}

char *getDirPrefix(const char *f)
{
  char *dirPrefix;

  if (strchr(f,'/'))
  {
    char *lastSlash, *ch;

    dirPrefix = strdup(f);
    lastSlash = strchr(dirPrefix,'/');
    while ((ch = strchr(lastSlash + 1,'/')))
    {
      lastSlash = ch;
    }
    *(lastSlash + 1) = '\0'; 
  }
  else
  {
    dirPrefix = "";
  }

  return dirPrefix;
}

static void addArchive(Cmdline_t *c,Arg_t *a)
{
  char *dirPrefix;
  char cmdline[1024];
  char buf[1024];
  FILE *ar;
  char *ch;
  int rc;

  assert(a->inputType == INPUT_IS_ARCHIVE);
  if (debug >= DEBUG_GORY_DETAILS)
    fprintf(debugf,"Adding the list of objects for %s now...\n",
            a->s);

  dirPrefix = getDirPrefix(a->s);

  sprintf(cmdline,"ar -t %s",a->s);
  ar = popen(cmdline,"r");
  if (!ar)
  {
    perror(cmdline);
    exit(1);
  }
  while (!ferror(ar) && !feof(ar))
  {
    ch = fgets(buf,sizeof(buf),ar);
    if (ch)
    {
      if (buf[strlen(buf) - 1] == '\n')
        buf[strlen(buf) - 1] = '\0';

      if (!strcmp(buf,"__.SYMDEF"))
      {
        /* not a real member; skip it */
      }
      else
      {
        if (debug >= DEBUG_GORY_DETAILS)
          fprintf(debugf,"archive member `%s'\n",buf);

        addArg(c,dirPrefix);
/*KLUDGE!!!!!!!!!!!!*/
        addArg(c,"objs/");
/*END KLUDGE!!!!!!!!*/
        addArg(c,buf);
        addArg(c," ");
      }
    }
  }
  rc = pclose(ar);
  if (rc)
  {
    fprintf(stderr,"`%s' -> %d\n",
            cmdline,rc);
    exit(1);
  }
}

static void addLarchive(Cmdline_t *c,Arg_t *a)
{
  char *dirPrefix;
  Larchive_t la;
  int cur;

  assert(a->inputType == INPUT_IS_LARCHIVE);
  if (debug >= DEBUG_GORY_DETAILS)
    fprintf(debugf,"Adding the list of objects for %s now...\n",
            a->s);

  dirPrefix = getDirPrefix(a->s);

  loadLarchive(&la,a->s);
  if (debug >= DEBUG_GORY_DETAILS)
    dumpLarchive(&la);

  cur = 0;
  while (cur < la.numInputs)
  {
    /* hackola! */
    if (!strcmp(la.inputs[cur],"main.o"))
    {
      /* don't put main.o in the dll; it is stand-alone */
    }
    else
    {
      addArg(c,dirPrefix);
      addArg(c,la.inputs[cur]);
      addArg(c," ");
    }
    ++cur;
  }
}

static int buildMain(Parms_t *p)
{
  int rc = 0;
  int orc;
  Cmdline_t c = {0};
  int curArg;
  const char *extraLflags;

  extraLflags = getenv("LIBTOOL_LFLAGS");

  /* First, build the dll. */

  curArg = 0;
  while (curArg < p->numArgs &&
         strcmp(p->args[curArg].s,"-o"))
  {
    if (curArg == 1 && extraLflags)
    {
      addArg(&c,extraLflags);
      addArg(&c," ");
    }
    addArg(&c,p->args[curArg].s);
    addArg(&c," ");
    ++curArg;
  }

  assert(!strcmp(p->args[curArg].s,"-o"));
  ++curArg;
  ++curArg;

  addArg(&c,"-Wl,DLL ");
  addArg(&c,"-o " CORE_DLL " ");

  /* Now, process the input files... */

  while (curArg < p->numArgs)
  {
    switch(p->args[curArg].inputType)
    {
      case INPUT_IS_OBJ:
      case INPUT_IS_LOBJ:
        addArg(&c,p->args[curArg].realInput);
        addArg(&c," ");
        break;
      case INPUT_IS_ARCHIVE:
        addArchive(&c,&p->args[curArg]);
        break;
      case INPUT_IS_LARCHIVE:
        addLarchive(&c,&p->args[curArg]);
        break;
      case INPUT_IS_IGNORED:
        /* Skip this; we don't care about it for one reason or another. */
        break;
      case INPUT_IS_OPTION:
        addArg(&c,p->args[curArg].s); 
        addArg(&c," ");
        break;
      default:
        fprintf(stderr,"Unexpected input type %d at %d\n",
                p->args[curArg].inputType,__LINE__);
        exit(1);
    }
    ++curArg;
  }

  rc = runCmd(p,&c);

  if (!rc)
  {
    memset(&c,0,sizeof(c));
    addArg(&c,"cc ");
    if (extraLflags)
    {
      addArg(&c,extraLflags);
      addArg(&c," ");
    }
    addArg(&c,"-g -Wl,DLL -o httpd server/main.o " CORE_X);
  }

  if (!rc)
  {
    rc = runCmd(p,&c);
  }

  return rc;
}

static int buildExe(Parms_t *p)
{
  int rc = 0;

  assert(p->mode == LINK);
  assert(p->outputType == OUTPUT_IS_EXE);

  if (p->buildingDll && !strcmp(p->target,"httpd"))
  {
    rc = buildMain(p);
  }
  else
  {
    /*
     * simple link-edit: just run the specified command
     */

    Cmdline_t c = {0};
    int curArg;
    const char *extraLflags;

    extraLflags = getenv("LIBTOOL_LFLAGS");

    curArg = 0;
    while (curArg < p->numArgs)
    {
      if (curArg == 1 && extraLflags)
      {
        addArg(&c,extraLflags);
        addArg(&c," ");
      }
      if (curArg >= p->firstInput)
      {
        switch(p->args[curArg].inputType)
        {
	  case INPUT_IS_IGNORED:
            break;
          case INPUT_IS_OPTION:
            addArg(&c,p->args[curArg].s);
            break;
	  default:
            addArg(&c,p->args[curArg].realInput);
        }
      }
      else
        addArg(&c,p->args[curArg].s);
      addArg(&c," ");
      ++curArg;
    }

    if (!rc)
    {
      rc = runCmd(p,&c);
    }
  }

  return rc;
}

static int makeTimestamp(Parms_t *p,const char *inputFile)
{
  int rc = 0;
  char filename[200];
  char *extension;
  Cmdline_t c = {0};

  assert(strlen(inputFile) + 1 < sizeof(filename));
  strcpy(filename,inputFile);
  extension = filename + strlen(filename) - strlen(".c");
  if (strcmp(extension,".c"))
  {
    fprintf(stderr,
            "program failure at %d: filename `%s', extension `%s'\n",
            __LINE__,filename,extension);
    exit(1);
  }
  strcpy(extension,".lo");
  addArg(&c,"echo timestamp >");
  addArg(&c,filename);
  rc = runCmd(p,&c);
  return rc;
}

static int compile(Parms_t *p)
{
  int rc = 0;
  int curArg;
  Cmdline_t c = {0};
  const char *extraCflags, *realInput;

  extraCflags = getenv("LIBTOOL_CFLAGS");

  /*
   * simple compile: just run the specified command
   *
   * Currently we don't have any special processing for shlibtool.
   */

  curArg = 0;
  while (curArg < p->numArgs)
  {
    if (curArg == 1 && extraCflags)
    {
      addArg(&c,extraCflags);
      addArg(&c," ");
    }
    if (curArg >= p->firstInput)
    {
      switch(p->args[curArg].inputType)
      {
        case INPUT_IS_IGNORED:
	  break;
	case INPUT_IS_OPTION:
	  addArg(&c,p->args[curArg].s);
	  break;
	default:
          realInput = p->args[curArg].realInput;
          rc = editInputFile(p->args[curArg].realInput);
          if (rc)
          {
            fprintf(stderr,
                    PGM ": editInputFile(%s)->%d\n",
                    p->args[curArg].realInput,rc);
            exit(rc);
          }
          addArg(&c,p->args[curArg].realInput);
      }
    }
    else
      addArg(&c,p->args[curArg].s);
    addArg(&c," ");
    ++curArg;
  }

  if (!rc)
  {
    rc = runCmd(p,&c);
  }

  if (!rc)
  {
    rc = makeTimestamp(p,realInput);
  }

  return rc;
}

static int buildArchive(Parms_t *p)
{
  int rc = 0;
  int curArg;
  char *archiveName;
  Cmdline_t c = {0};

  /* turn foo.la into foo.a to create the archive name */
  archiveName = strdup(p->target);
  strcpy(archiveName + strlen(archiveName) - 3,".a"); 

  removeFile(archiveName,1);

  /* build ar command-line */

  addArg(&c,"ar -rs ");
  addArg(&c,archiveName);
  addArg(&c," ");
  curArg = p->firstInput;
  while (curArg < p->numArgs)
  {
    assert(p->args[curArg].inputType != INPUT_IS_OPTION);
    if (p->args[curArg].inputType != INPUT_IS_IGNORED)
    {
      addArg(&c,p->args[curArg].realInput);
      addArg(&c," ");
    }
    ++curArg;
  }

  rc = runCmd(p,&c);

  /* now add a .libs directory, and create a symlink in it to foo.a */
  
  if (!rc)
  {
    char oldPath[260], newPath[260];

    rc = mkdir(".libs", 0755);
    if (rc && errno != EEXIST)
    {
      perror("libtoolexe: buildArchive: mkdir");
      exit(rc);
    }
    strcpy(oldPath, "../");
    strcat(oldPath, archiveName);
    strcpy(newPath, ".libs/");
    strcat(newPath, archiveName);
       
    rc = symlink(oldPath, newPath);
    if (rc && errno != EEXIST)
    {
      perror("libtoolexe: buildArchive: symlink");
      exit(rc);
    }
    else
      rc = 0;
  }

  if (!rc)
  {
    /* now build the .la file, which contains a list of objects */
    FILE *la;

    la = fopen(p->target,"w");
    if (!la)
    {
      perror(p->target);
      exit(1);
    }
    fprintf(la,"input:");
    curArg = p->firstInput;
    while (curArg < p->numArgs)
    {
      if (p->args[curArg].inputType != INPUT_IS_IGNORED)
      {
        assert(p->args[curArg].inputType != INPUT_IS_OPTION);
        fprintf(la,"%s ",p->args[curArg].realInput);
      }
      ++curArg;
    }
    fprintf(la,"\n");
    fclose(la);
  }

  return rc;
}

static int parseCmdline(int argc,char **argv,Parms_t *p)
{
  int rc = 0;
  int curArg;
  int firstInputSet = 0;
  const char *modeStr;
  enum {NORM, TARGET} state = NORM;

  p->mode = 0; /* not a valid mode */
  curArg = 1;
  while (curArg < argc)
  {
    if (debug >= DEBUG_GORY_DETAILS)
      fprintf(debugf,"arg %d: %s\n",curArg,argv[curArg]);
   
    if (!strcmp(argv[curArg],"--from-shlibtool"))
    {
      p->fromShlibtool = 1;
    }
    else if (!strcmp(argv[curArg],"--silent"))
    {
      p->silent = 1;
    }
    else if (!strcmp(argv[curArg],"--quiet"))
    {
      p->silent = 1;
    }
    else if (!strcmp(argv[curArg],"--version"))
    {
      p->showVersion = 1;
    }
    else if (!strcmp(argv[curArg],"-rpath"))
    {
      ++curArg;
      p->rpath = argv[curArg];
    }
    else if (!strcmp(argv[curArg],"-version-info"))
    {
      ++curArg;
      p->version = argv[curArg];
    }
    else if (!strcmp(argv[curArg],"-module"))
    {
      p->module = 1;
    }
    else if (!strcmp(argv[curArg],"-avoid-version"))
    {
      p->avoidVersion = 1;
    } 
    else if (!memcmp(argv[curArg],"--mode=",7))
    {
      modeStr = argv[curArg] + 7;
      if (!strcmp(modeStr,"compile"))
        p->mode = COMPILE;
      else if (!strcmp(modeStr,"link"))
        p->mode = LINK;
      else if (!strcmp(modeStr,"install"))
        p->mode = INSTALL;
      else
      {
        fprintf(stderr,"unknown mode parm: %s\n",argv[curArg]);
        p->mode = UNKNOWN_MODE;
      }
    }
    else if (!strcmp(argv[curArg],"-export-dynamic")) 
    {
      p->exportDynamic = 1;
    }
    else
    {
      if (debug >= DEBUG_GORY_DETAILS)
        fprintf(debugf,"normal compile option: %s\n",argv[curArg]);
      assert(p->numArgs < MAX_ARGS);
      p->args[p->numArgs].s = argv[curArg];
      ++p->numArgs;

      /* KLUDGE!!! */
      /* hokey way to see if we're doing dll-able code */
      if (strstr(argv[curArg],"-Wc,DLL"))
        p->buildingDll = 1;
      /* END KLUDGE!!! */

      if (!strcmp(argv[curArg],"-o"))
      {
        assert(state == NORM);
        state = TARGET;
      }
      else if (state == TARGET)
      {
        size_t targetLen = strlen(argv[curArg]);

        state = NORM;
        p->firstInput = p->numArgs; /* input files come after target */
        firstInputSet = 1;
        assert(!p->target);
        p->target = argv[curArg];

        /* Figure out what type of target it is. */
        if (!strcmp(p->target + targetLen - 3,".la"))
        {
          p->outputType = OUTPUT_IS_ARCHIVE;
        }
        else if (!strchr(p->target,'.'))
          p->outputType = OUTPUT_IS_EXE;
        else
          p->outputType = UNKNOWN_OUTPUT;
      }
      else if (p->mode == COMPILE && 
               !firstInputSet &&
               curArg == argc - 1)
      {
        p->firstInput = p->numArgs - 1;
        firstInputSet = 1;
      }

      if (firstInputSet && (p->numArgs - 1) >= p->firstInput)
      {
        /* convert foo.lo into foo.o */
        if (strstr(argv[curArg],".lo"))
        {
          char *tmp;

          p->args[p->numArgs - 1].inputType = INPUT_IS_LOBJ;
          tmp = strdup(argv[curArg]);
          assert(tmp);
          strcpy(tmp + strlen(tmp) - 3,".o");
          p->args[p->numArgs - 1].realInput = tmp;
        }
        else if (strstr(argv[curArg],".o"))
        {
          p->args[p->numArgs - 1].inputType = INPUT_IS_OBJ;
          p->args[p->numArgs - 1].realInput = 
            p->args[p->numArgs - 1].s;
        }
        else if (strstr(argv[curArg],".la"))
        {
          char *tmp;

          p->args[p->numArgs - 1].inputType = INPUT_IS_LARCHIVE;
          tmp = strdup(argv[curArg]);
          assert(tmp);
          strcpy(tmp + strlen(tmp) - 3,".a");
          p->args[p->numArgs - 1].realInput = tmp;
        }
        else if (strstr(argv[curArg],".a"))
        {
          p->args[p->numArgs - 1].inputType = INPUT_IS_ARCHIVE;
          p->args[p->numArgs - 1].realInput = 
            p->args[p->numArgs - 1].s;
        }
        else if (strstr(argv[curArg],".h"))
        {
          p->args[p->numArgs - 1].inputType = INPUT_IS_IGNORED;
        }
        else if (strstr(argv[curArg],".c"))
        {
          p->args[p->numArgs - 1].inputType = INPUT_IS_SOURCE;
          p->args[p->numArgs - 1].realInput = 
            p->args[p->numArgs - 1].s;
        }
        else
        {
          /* 
           * INPUT_IS_OPTION is a kludge for handling options which
           * come in the middle of the list of input files.
           */
          p->args[p->numArgs - 1].inputType = INPUT_IS_OPTION;
        }
      }
      else
        p->args[p->numArgs - 1].inputType = NOT_INPUT;
    }
    ++curArg;
  }

  if (p->mode == 0 &&
      p->showVersion)
  {
    p->mode = SHOWVERSION;
  }

  if (!firstInputSet)
  {
    /* no input files specified */
    p->firstInput = p->numArgs;
  }
  
  return rc;
}

static int link(Parms_t *p)
{
  int rc;

  switch(p->outputType)
  {
    case OUTPUT_IS_ARCHIVE:
      if (p->fromShlibtool)
        rc = shlibtoolLink(p);
      else
        rc = buildArchive(p);
      break;
    case OUTPUT_IS_EXE:
      assert(!p->fromShlibtool);
      rc = buildExe(p);
      break;
    default:
      fprintf(stderr,
              "link(): unhandled output type %d\n",
              p->outputType);
      exit(1);
  }

  return rc;
}

static int version(Parms_t *p)
{
  assert(p->mode == SHOWVERSION);

  /*
   * Note: We don't print "OS/390" in the following message because
   *       that throws off Apache's sed code to strip the version
   *       number out of the output.
   */

  printf(PGM ": This is libtool 1.3.4 for OS/three-ninety.\n"
         "It acts enough like GNU libtool to allow Apache to be built.\n");
  return 0;
}

static int install(Parms_t *p)
{
  int rc = 0;
  int cur;
  Cmdline_t c = {0};
  char *so;
  Larchive_t la;

  assert(p->numArgs == 3);
  assert(p->fromShlibtool);
  assert(!strcmp(p->args[0].s,"cp"));

  so = strdup(p->args[1].s);
  strcpy(strstr(so,".la"),".so");

  loadLarchive(&la,p->args[1].s);

  addArg(&c,"cc -Wl,DLL -o ");
  addArg(&c,so);
  addArg(&c," ");
  cur = 0;
  while (cur < la.numInputs)
  {
    addArg(&c,la.inputs[cur]);
    addArg(&c," ");
    ++cur;
  }
  addArg(&c,"../../" CORE_X);

  rc = runCmd(p,&c);

  if (!rc)
  {
    memset(&c,0,sizeof(c));
    addArg(&c,"cp ");
    addArg(&c,so);
    addArg(&c," ");
    addArg(&c,p->args[2].s);

    rc = runCmd(p,&c);
  }
  
  return rc;
}

int main(int argc,char **argv)
{
  int rc;
  Parms_t parms = {0};

  debugf = stdout;
  debug = getenv("LIBTOOLDEBUG") != NULL;
  if (debug)
    debug = atoi(getenv("LIBTOOLDEBUG"));

  if (argc == 1)
  {
    fprintf(stderr,
            PGM ": Please run OS/390 libtool with some parameters!\n");
    exit(1);
  }

  rc = readCfgFile();
  if (rc)
  {
    fprintf(stderr,PGM ": readCfgFile()->%d\n",rc);
    exit(1);
  }
 
  rc = parseCmdline(argc,argv,&parms);
  if (rc)
  {
    fprintf(stderr,"parseCmdline()->%d\n",
            rc);
    exit(rc);
  }

  if (debug >= DEBUG_OVERVIEW)
    dumpParms(&parms);

  switch(parms.mode)
  {
    case LINK:
      rc = link(&parms);
      break;
    case COMPILE:
      rc = compile(&parms);
      break;
    case SHOWVERSION:
      rc = version(&parms);
      break;
    case INSTALL:
      rc = install(&parms);
      break;
    default:
      fprintf(stderr,
              PGM ": support needed for mode %s/output %s\n",
              modeStr(parms.mode),outputTypeStr(parms.outputType));
      exit(999);
  }

#ifdef OLD
      /* normal command was done here */
      if (!rc)
        rc = runCmds(target);
      exit(rc);
#endif

  if (rc)
    fprintf(stderr,PGM ": returning error code %d...\n",rc);

  return rc;
}
