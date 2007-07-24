/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2002 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

static const char rcsid[] = "$Id$";

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>
#include <sys/stat.h>

#ifdef __BEOS__
#define PLATFORM "BeOS"
#define BEOS_BUILD       1
#define OS390_BUILD      0
#define AR_ADD_WITH_REPLACE    "ar crs "
#define SUPPORT_DLL_SPLIT 0
#endif

#ifdef __MVS__
/* Note: We really don't want __MVS__ everywhere because we may need to
 *       support a TPF cross-compile in the future.
 */
#define PLATFORM "z/OS"
#define BEOS_BUILD       0
#define OS390_BUILD      1
#define AR_ADD_WITH_REPLACE    "ar -rs "
/* we support splitting main() from the rest of the code 
 * to get a small executable plus a big dll. */
#define SUPPORT_DLL_SPLIT 1
#define _POSIX_SOURCE
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#undef _POSIX_SOURCE
#endif

#ifndef PLATFORM
#error Please define the PLATFORM string and other system-dependent symbols.
#endif

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

/* Forward declarations.... */

char *getDirPrefix(const char *f);

#define MAX_INPUTS 1000

typedef struct
{
  char *fname;
  int numInputs;
  char *inputs[MAX_INPUTS];
  char *prefix;
  char *staticLib;
  char *sharedLib;
  char *shLink; /* what we need to pass on the link line... */
  int installed;
  char *installPath;
  int linkShared;
} Larchive_t;

typedef struct
{
  char *s; 
  /* the following fields are always set if this is an input file */
  const char *realInput;
  Larchive_t arch; /* possible pointer to an Larchive_t */
  /* Meanings....
   *    INPUT_IS_OBJ         Normal object (.o)
   *    INPUT_IS_LOBJ        This is a libtool object (.lo)
   *    INPUT_IS_ARCHIVE     It's a static archive (.a)
   *    INPUT_IS_LARCHIVE    It's a libtool archive (.la)
   *    INPUT_IS_SOURCE      This is a source file (.c or .cc)
   *    INPUT_IS_IGNORED     Ignore this input (normally .h file)
   *    NOT_INPUT            This option wasn't an input
   *    INPUT_IS_OPTION      It's an option
   *    INPUT_IS_TARGET      This option was the build target
   */     
  enum {INPUT_IS_OBJ = 100, INPUT_IS_LOBJ, INPUT_IS_ARCHIVE,
        INPUT_IS_LARCHIVE, INPUT_IS_SOURCE, INPUT_IS_IGNORED, NOT_INPUT,
        INPUT_IS_OPTION, INPUT_IS_TARGET} inputType;
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
  unsigned int linkStatic : 1;
  unsigned int bldSharedObj : 1;
#if SUPPORT_DLL_SPLIT
  const char *main_obj;
  const char *core_dll;
#endif
  const char *target;
  const char *lt_target;
  const char *rpath;
  const char *version;
  enum {COMPILE=3, LINK, SHOWVERSION, INSTALL, UNKNOWN_MODE} mode;
  enum {OUTPUT_IS_OBJ=9, OUTPUT_IS_ARCHIVE, OUTPUT_IS_EXE, UNKNOWN_OUTPUT} outputType;
  Arg_t args[MAX_ARGS];
  int numArgs;
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
      return "(Libtool archive)";
    case INPUT_IS_SOURCE:
      return "SOURCE";
    case INPUT_IS_IGNORED:
      return "(ignored)";
    case INPUT_IS_OPTION:
      return "(compiler option)";
    case INPUT_IS_TARGET:
      return "(build target)";
    case NOT_INPUT:
      return "(not input)";
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
         p->linkStatic    ? "static "         : "",
         p->module        ? "module "         : "",
         p->showVersion   ? "show-version "   : "",
         p->avoidVersion  ? "avoid-version "  : "",
         p->bldSharedObj  ? "bldSharedObj "   : "");
  printf("Compiler arguments:\n"); /* this is useless now... */
  printf("Input parameters:\n");
  curArg = 0; 
  while (curArg < p->numArgs)
  {
    printf("\t%-18s %-50s %s\n",
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
      case '"':
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

static void _addArg(Cmdline_t *c,const char *add, int escape)
{
  size_t addLen;

  if (debug >= DEBUG_GORY_DETAILS)
    fprintf(stderr, "_addArg(,%s,%d)\n",
            add, escape);

  if (escape)
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

static void addArg(Cmdline_t *c,const char *add)
{
  _addArg(c, add, 1);
}

static void addArgUnescaped(Cmdline_t *c,const char *add)
{
  _addArg(c, add, 0);
}
#if SUPPORT_DLL_SPLIT
static char *getXfile(const char *name)
{
  char xname[1024];
  if (name == NULL) return;
  xname[0]='\0';
  strcpy(xname, name);
  if (strstr(xname, ".a") != NULL) {
     strcpy(strstr(xname, ".a"), ".x");
     return strdup(xname); 
  }
  if (strstr(xname, ".la") != NULL) {
     strcpy(strstr(xname, ".la"), ".x ");
     return strdup(xname); 
  }
  return xname;  
}
static void addXfiles(Cmdline_t *c,const char *larchd)
{  
  char *xname;
  struct dirent *entry;
  DIR *ldir;
  char buf[1024]="";

  if ((ldir = opendir(larchd)) == NULL) {
      fprintf(debugf, "Warning- could not open directory %s to create .x files errno %d\n",larchd,errno);
      errno=0;
      return;
  }

  while ((entry = readdir(ldir)) != NULL) {
      if (entry->d_name[0] != '.') {
          xname = strdup(entry->d_name); 
         if (!strncmp(xname+strlen(xname)-2, ".x", 2)) {
              if (debug >= DEBUG_GORY_DETAILS)
                fprintf(stderr, " adding x file %s from a directory with -L\n",xname); 
              strcpy(buf, '\0');
              strcpy(buf, larchd);
              strcat(buf, "/");
              strcat(buf, xname);
              addArg(c, buf);
              addArg(c, " ");  
         }
      }
  } 

  if (closedir(ldir) == -1) {
      assert(!errno);
  }
  return;
}
#endif /* SUPPORT_DLL_SPLIT */
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


/******************************************************************************
 * routines for manipulating the Larchive_t structure...
 */
 
static void _addStaticArg(Cmdline_t *c, Larchive_t *la)
{
  if (debug >= DEBUG_GORY_DETAILS)
    fprintf(debugf, "trying to add static object from %s\n", la->fname);

  if (la->installed)
  {
    addArg(c, la->installPath);
    addArg(c, "/");
  }
  else
    addArg(c, la->prefix);
  addArg(c, la->staticLib);
  addArg(c, " ");
}

static void _addSharedArg(Cmdline_t *c, Larchive_t *la, Parms_t *p)
{
  char xname[1024];
  if (debug >= DEBUG_GORY_DETAILS)
    fprintf(debugf, "trying to add shared object from %s\n", la->fname);

#if BEOS_BUILD
  addArg(c, "-L");
  if (la->installed){
    addArg(c, la->installPath);
  } else {
    char *tmp = strdup(la->prefix);
    if (*(tmp + strlen(tmp) -1) == '/')
      *(tmp + strlen(tmp) -1) = '\0';
    /* if we don't have a prefix and we're not installed, assume it's this directory... */
    if (strlen(tmp) == 0)
      strcpy(tmp, ".\0");
    addArg(c, tmp);
    /* as we're not installed, add an rpath if we're building an exe... */
    if (p->outputType == OUTPUT_IS_EXE){
      addArg(c, " -Xlinker -rpath ");
      addArg(c, tmp);
    }
  }
#endif
#if SUPPORT_DLL_SPLIT
  xname[0]='\0';
  strcpy(xname, la->fname);
  /*assert(strstr(xname,".la"));*/
  if (strstr(xname,".la") != NULL) {
    strcpy(strstr(xname, ".la"), ".x ");
  }
  addArg(c, xname);
  addArg(c, " ");
#else
  addArg(c, "-l");
  addArg(c, la->shLink);
  addArg(c, " ");
#endif /* SUPPORT_DLL_SPLIT */
}

static char *_getstrdata(char *line)
{
    char *ptr = strchr(line, '=') + 1;
    while (isspace(*ptr))
      ptr++;
    if (*ptr == '\''){
        /* need to remove the ' */
        char *eptr = ptr++;
        eptr = strchr(ptr, '\'');
        if (eptr)
            *eptr = '\0';
    }

    if (strlen(ptr) == 0) {
        return NULL;
    }

    return ptr;
}

static void dumpLarchive(Larchive_t *la)
{
  fprintf(debugf,"Libtool archive file %s\n", la->fname);
  fprintf(debugf,"\tstatic library : %s\n", la->staticLib ? la->staticLib : "(none)");
  fprintf(debugf,"\tshared library : %s\n", la->sharedLib ? la->sharedLib : "(none)");
  fprintf(debugf,"\tinstalled      : %s\n", la->installed ? "yes" : "no");
  fprintf(debugf,"\tinstall path   : %s\n", la->installPath);
}

static int isLarchiveStatic(Larchive_t *la)
{
  /* we return 1 if we have a static library available... */
  if (la->staticLib)
    return 1;
  else
    return 0;
}

/* These might be a bit over the top... */
static void _getLarchiveStatic(char *rv, Larchive_t *la)
{
  if (la->installed)
    strcpy(rv, la->installPath);
  else
    strcpy(rv, la->prefix);
  strcat(rv, la->staticLib);
}

static void _getLarchiveShared(char *rv, Larchive_t *la)
{
  *rv = '\0';
  if (la->installed)
    strcpy(rv, la->installPath);
  else
    strcpy(rv, la->prefix);
  strcat(rv, la->sharedLib);
}
  
static void readLarchive(Larchive_t *la, const char *fname, int must_exist)
{
  FILE *in;
  char *inputline = malloc(100000);
  char *ch, *tmpch;
  char *parm;

  if (debug >= DEBUG_GORY_DETAILS)
    fprintf(debugf, "Reading the libtool archive file %s\n", fname);

  memset(la,0,sizeof(*la));

  la->fname = strdup(fname);
  la->prefix = strdup(getDirPrefix(fname));
  in = fopen(fname,"r");
  if (!in && must_exist)
  {
    fprintf(stderr, "larchive file %s could not be opened: %s\n",
            fname, strerror(errno));
    exit(1);
  }

  if (in)
  {
    while (!ferror(in) && !feof(in))
    {
      ch = fgets(inputline,100000,in);
      if (ch)
      {
        if (inputline[strlen(inputline) - 1] != '\n')
        {
          fprintf(stderr,"line too big in %s\n",fname);
          exit(1);
        }
        inputline[strlen(inputline) - 1] = '\0';
        if (inputline[0] == '#' || inputline[0] == '\0') {
          continue;
        }
        if (strstr(inputline,"library_names")){
            parm = _getstrdata(inputline);
            if (parm) {
                la->sharedLib = strdup(parm);
                la->shLink = strdup(la->sharedLib) + 3;
                strcpy(strstr(la->shLink, ".so"), "\0");
            }
            continue;
        }
        if (strstr(inputline,"old_library")) {
            parm = _getstrdata(inputline);
            if (parm) {
                la->staticLib = strdup(parm);
            }
            continue;
        }
        if (strstr(inputline,"installed=")) {
            parm = _getstrdata(inputline);
            if (parm) {
                la->installed = (strstr(parm,"yes") ? 1 : 0);
            }
            continue;
        }
        if (strstr(inputline,"libdir=")) {
            parm = _getstrdata(inputline);
            if (parm) {
                la->installPath = strdup(parm);
            }
            continue;
        }
#if SUPPORT_DLL_SPLIT
        if (strstr(inputline,"addsharedlib=")) {
            parm = _getstrdata(inputline);
            if (parm) {
                la->linkShared = (strstr(parm,"yes") ? 1 : 0);
            }
            continue;
        }
#endif
        if (!memcmp(inputline,"input:",6))
        {
          ch = inputline + 6;
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
        }
        continue;
        fprintf(stderr,
                "syntax error in %s: %s\n",
                fname,inputline);
      }
    }
    fclose(in);
  }

  free(inputline); 
}

static void writeLarchive(Larchive_t *la)
{
  FILE *lafile;
  int curInput;

  if (debug >= DEBUG_GORY_DETAILS)
    fprintf(debugf, "Writing the libtool archive file %s\n", la->fname);

  lafile = fopen(la->fname,"w");
  if (!lafile)
  {
    fprintf(stderr,"couldn't create %s: %s\n",la->fname,strerror(errno));
    exit(1);
  }

  fprintf(lafile,
      "# %s - a libtool library file\n"
      "# Generated by " PGM " for " PLATFORM "\n\n# Don't delete this file...\n",
      la->fname);
  
  fprintf(lafile, 
          "# The name that we can dlopen() or dllload() or whatever.\n"
          "dlname='%s'\n\n", la->sharedLib ? la->sharedLib : "");
  fprintf(lafile, 
      "# Names of this library.\n"
      "library_names='%s'\n"
      "\n", la->sharedLib ? la->sharedLib : "");
  fprintf(lafile, 
      "# The name of the static archive.\n"
      "old_library='%s'\n"
      "\n", la->staticLib ? la->staticLib : "");
  fprintf(lafile, 
      "# Is this library installed?\n"
      "installed=%s\n"
      "\n", la->installed ? "yes" : "no");
  fprintf(lafile, 
      "# Directory that this library needs to be installed in\n"
      "libdir='%s'\n"
      "\n", la->installPath ? la->installPath : "");
#if SUPPORT_DLL_SPLIT
  /* Kluge to be replaced with setting la->linkShared iff
   * xyz.la is in a directory specified with a -L parm, i.e.
   * turn on linkShared in libaprutil-1.la because -L/..../srclib/apr-util */
  if (strstr(la->fname,"libapr") ||
      strstr(la->fname,"libparutil") ||
      strstr(la->fname,"libexpat")) {
      fprintf(lafile, 
      "# Indicate that this shared library needs to be added to main\n"
      "addsharedlib=%s\n"
      "\n", "yes");
  } else {
       fprintf(lafile, 
      "# Indicate that this shared library needs to be added to main\n"
      "addsharedlib=%s\n"
      "\n", "no");
  }     
#endif

  if (la->numInputs > 0)
  {
    fprintf(lafile,"input:");
    curInput = 0;
    while (curInput < la->numInputs)
    {
      fprintf(lafile,"%s ",la->inputs[curInput]);
      ++curInput;
    }
    fprintf(lafile,"\n");
  }

  fclose(lafile);
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

static void addLarchive(Cmdline_t *c,Arg_t *a, Parms_t *p)
{
  Larchive_t *la = &(a->arch);

  assert(a->inputType == INPUT_IS_LARCHIVE);
  if (debug >= DEBUG_GORY_DETAILS)
  {
    fprintf(debugf,"Adding the list of objects for libtool archive %s now...\n",
            a->s);
    dumpLarchive(la);
  }

  /* serious problem if at least one of these isn't set */
  assert(la->staticLib || la->sharedLib);

#if SUPPORT_DLL_SPLIT
  if (p->main_obj) /* building main executable + special dll */
  {
    /* unfortunately, we need to provide the names of the .o files on the
     * link invocation so that apachecore.x is built;
     * when we put .a files on the link invocation the linker will only
     * search for code in them needed by the .o files; it won't blindly
     * include the code in the .a in the file being built
     */

    int cur;
    char *dirPrefix;
    char curdir[1024];
    char curname[1024];

    if (debug >= DEBUG_GORY_DETAILS)
      fprintf(stderr,
              "building main executable + special dll\n");

    dirPrefix = getDirPrefix(a->s);
    getcwd(curdir, sizeof curdir - 1);

    if (la->linkShared) {
      curname[0] = '\0';
      if (dirPrefix[0] != '/') {
        strcat(curname, curdir);
        strcat(curname, "/");
      }
      strcat(curname, dirPrefix);
      addXfiles(c, curname); 
      return;
    } 
    cur = 0;
    while (cur < la->numInputs)
    {
      curname[0] = '\0';
      if (dirPrefix[0] != '/') {
        strcat(curname, curdir);
        strcat(curname, "/");
      }
      strcat(curname, dirPrefix);
      strcat(curname, la->inputs[cur]);
 
      if (!strcmp(curname, p->main_obj))
      {
        /* don't put this object in the dll; it is stand-alone */
        if (debug >= DEBUG_GORY_DETAILS)
        {
          fprintf(debugf,"skipping %s%s... it has main()...\n",
                  dirPrefix, la->inputs[cur]);
        }
      }
      else
      {
        if (*(la->inputs[cur]) != '/') {
          /* if it's not an absolute path, add the directory prefix */
          addArg(c,dirPrefix);
        }
        addArg(c,la->inputs[cur]);
        addArg(c," ");
      }
      ++cur;
    }	
    return;
  }
#endif /* SUPPORT_DLL_SPLIT */

  /* we only have a static library... */
  if (la->staticLib && !la->sharedLib) {
      _addStaticArg(c, la);
  }
  /* we only have a shared library... */
  if (!la->staticLib && la->sharedLib) {
      _addSharedArg(c, la, p);
  }
  /* we have both a shared and static library...
   *
   * What do we do here...????????????
   */
  if (la->staticLib && la->sharedLib) {
      /* add the shared library... */
      if (p->linkStatic) {
          _addStaticArg(c, la);
      }
      else {
          _addSharedArg(c, la, p);
      }
  }
  
  if (debug >= DEBUG_GORY_DETAILS)
    dumpLarchive(la);
}

static int shlibtoolLink(Parms_t *p)
{
  int rc = 0;
  FILE *la;
  char *intendedSo = strdup(p->target);
  char *dotla, *archiveName;
  int curArg;
  Cmdline_t c = {0};
  Larchive_t larch;
  
  /* turn foo.la into foo.so to create the archive name */
  readLarchive(&larch, p->target, 0);
  archiveName = strdup(p->target);
  strcpy(intendedSo + strlen(intendedSo) - 3,".so"); 

  removeFile(intendedSo,1);

  curArg = 0;

#if OS390_BUILD
  addArg(&c,p->args[curArg].s);
  addArg(&c," ");
  ++curArg;

  addArg(&c,"-Wl,DLL ");
  addArg(&c,"-o ");
  addArg(&c,intendedSo);
  addArg(&c," ");
#endif

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
        addLarchive(&c,&p->args[curArg], p);
        break;
      case INPUT_IS_IGNORED:
      case NOT_INPUT:
      case INPUT_IS_TARGET: /* we may have a .la... */
        /* Skip this; we don't care about it for one reason or another. */
        curArg++;
        break;
      case INPUT_IS_OPTION:
        addArg(&c,p->args[curArg].s); 
        addArg(&c," ");
#if SUPPORT_DLL_SPLIT
        if (strstr(p->args[curArg].s,"-L")) {
          addXfiles(&c,p->args[curArg].s + 2);
        }
#endif
        break;
      default:
        break;
    }
    ++curArg;
  }

#if BEOS_BUILD
  addArg(&c," -nostart -o ");
  addArg(&c,intendedSo);
  addArg(&c," -Wl,-soname,");
  addArg(&c,intendedSo);
#endif
#if SUPPORT_DLL_SPLIT
  if (p->core_dll)
  {
    char *core_x;

    core_x = strdup(p->core_dll);
    strcpy(strstr(core_x,".dll"),".x");

    addArg(&c,core_x);
  }
#endif /* SUPPORT_DLL_SPLIT */

  /* Run the command and make a library!! */
  rc = runCmd(p,&c);

  /*
   * Create the output file
   */
  if (!rc) {
    larch.sharedLib = strdup(intendedSo);
    larch.installPath = strdup(p->rpath);
    writeLarchive(&larch);
  }

  return rc;
}

#if SUPPORT_DLL_SPLIT
static int buildMain(Parms_t *p)
{
  int rc = 0;
  int orc;
  Cmdline_t c = {0};
  Cmdline_t linkMainCmd = {0};
  int curArg;
  const char *extraLflags;
  char *core_x;
  int debug = 0;
  char *nametoadd;
  Larchive_t larch;

  assert(p->core_dll);
  core_x = strdup(p->core_dll);
  strcpy(strstr(core_x,".dll"),".x");

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
      /* We need to use same options on the link of the main executable.
       */
      addArg(&linkMainCmd,extraLflags);
      addArg(&linkMainCmd," ");
    }
    addArg(&c,p->args[curArg].s);
    addArg(&c," ");
    /* We need to use same options on the link of the main executable.
     */
    addArg(&linkMainCmd,p->args[curArg].s);
    addArg(&linkMainCmd," ");
    ++curArg;
  }

  assert(!strcmp(p->args[curArg].s,"-o"));
  ++curArg;
  ++curArg;

  addArg(&c,"-Wl,DLL ");
  addArg(&c,"-o ");
  addArg(&c,p->core_dll);
  addArg(&c," ");

  /* Now, process the input files... */

/* NB - you'll need to look at the INPUT_IS_TARGET flag for this
 * block!
 */
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
        readLarchive(&larch, p->args[curArg].s , 0);
        if (larch.linkShared) { 
            nametoadd = getXfile(p->args[curArg].s);
            assert(nametoadd);
            addArg(&c,nametoadd);
            addArg(&c," ");
            addArg(&linkMainCmd,nametoadd);
            addArg(&linkMainCmd," ");
        } else { 
            addLarchive(&c,&p->args[curArg], p); 
        } 
        break;
      case INPUT_IS_IGNORED:
      case NOT_INPUT:
        /* Skip this; we don't care about it for one reason or another. */
        break;
      case INPUT_IS_OPTION:
        addArg(&c,p->args[curArg].s); 
        addArg(&c," ");
        if (!strcmp(p->args[curArg].s,"-g"))
          debug = 1;
        /* We need to use same options on the link of the main executable.
         */
        addArg(&linkMainCmd,p->args[curArg].s);
        addArg(&linkMainCmd," ");
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
    addArg(&linkMainCmd,"-Wl,DLL -o ");
    addArg(&linkMainCmd,"httpd");
    addArg(&linkMainCmd," ");
    addArg(&linkMainCmd,p->main_obj);
    addArg(&linkMainCmd," ");
    addArg(&linkMainCmd,core_x);
    rc = runCmd(p,&linkMainCmd);
  }

  return rc;
}
#endif /* SUPPORT_DLL_SPLIT */

static int buildExe(Parms_t *p)
{
  int rc = 0;
  Cmdline_t c = {0};
  int curArg;
  const char *extraLflags;

  assert(p->mode == LINK);
  assert(p->outputType == OUTPUT_IS_EXE);

#if SUPPORT_DLL_SPLIT
  if (p->main_obj)
    return buildMain(p);
#endif /* SUPPORT_DLL_SPLIT */

  /*
   * simple link-edit: just run the specified command
   */

  extraLflags = getenv("LIBTOOL_LFLAGS");

  curArg = 0;
  while (curArg < p->numArgs)
  {
    if (curArg == 1 && extraLflags)
    {
      addArg(&c,extraLflags);
      addArg(&c," ");
    }
    switch(p->args[curArg].inputType)
    {
      case INPUT_IS_IGNORED:
        break;
      case INPUT_IS_OPTION:
      case INPUT_IS_TARGET: /* we don't mung the target... */
        addArg(&c,p->args[curArg].s);
#if SUPPORT_DLL_SPLIT
        if ((!p->linkStatic) && (strstr(p->args[curArg].s,"-L"))) {
          addArg(&c," ");
          addXfiles(&c,p->args[curArg].s + 2);
        }
#endif
        break;
      case INPUT_IS_LARCHIVE:
        addLarchive(&c, &p->args[curArg], p);
        break;
      case NOT_INPUT:
        addArg(&c,p->args[curArg].s);
        break;
      default:
        addArg(&c,p->args[curArg].realInput);
    }
    addArg(&c," ");
    ++curArg;
  }

  if (!rc)
  {
    rc = runCmd(p,&c);
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
  if (strstr(filename, ".cc")){
    extension = filename + strlen(filename) - strlen(".cc");
  } else {
    extension = filename + strlen(filename) - strlen(".c");
  }
  if (strcmp(extension,".c") && strcmp(extension,".cc"))
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
#if OS390_BUILD
    if (curArg == 1)
    {
      /* We don't truly need this unless objects can be put in DLLs, but it never
       * hurts as far as I can tell.
       *
       * This argument is silently ignored by cc if added after the input .c file,
       * so that is why we're adding it here instead of where we add -fPIC on BeOS.
       */
      addArg(&c, "-Wc,DLL,EXPORTALL ");
    }
#endif

    switch(p->args[curArg].inputType)
    {
      case INPUT_IS_IGNORED:
        break;
      case INPUT_IS_OPTION:
        if (strstr(p->args[curArg].s,"c++")){
          addArg(&c,"gcc");
        } 
        else {
          addArg(&c,p->args[curArg].s);
	}
	break;
      case INPUT_IS_TARGET:
        /* this is the foo.lo from "-o foo.lo" */
        {
          char buf[128];
          strncpy(buf, p->args[curArg].s, sizeof(buf) - 1);
          buf[sizeof(buf) - 1] = '\0';
          buf[strlen(buf) - 2] = 'o';
          buf[strlen(buf) - 1] = '\0';
          addArg(&c, buf);
        }
        break; 
      default:
        realInput = p->args[curArg].realInput;
        addArg(&c,p->args[curArg].realInput);
    }
    addArg(&c," ");
    ++curArg;
  }

#if BEOS_BUILD
  /* how do we actually know if we need to build PIC code???  Must be some way
   * from the command line.  Need to look at the GNU libtool code for more info.
   *
   * OS/390 note:
   * analogous problem for us is that we need to use special compile flags to
   * export our symbols; I don't think shlibtool is always used in the
   * cases where we need to export our symbols.  For that reason, Apache/APR
   * build is hacked to add the right options...
   * maybe we should change libtool instead to always add the export-symbols
   * option... I don't think it hurts anything
   *
   * This doesn't work at present as we're not setting the fromShlibtool
   * option correctly.
   */
  if (p->fromShlibtool)
    addArg(&c, "-fPIC ");
#endif
  
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
  int curArg = 0;
  int use_subdir = 0;
  char *archiveName;
  Cmdline_t c = {0}, cmd = {0};
  Larchive_t larch;

  /* turn foo.la into foo.a to create the archive name */
  readLarchive(&larch, p->target, 0);
  archiveName = strdup(p->target);
  strcpy(archiveName + strlen(archiveName) - 3,".a"); 

  removeFile(archiveName,1);

#ifndef SUPPORT_DLL_SPLIT
  /* Check if we need to use a seperate sub directory for the objects... */
  while (curArg < p->numArgs)
  {
    if (p->args[curArg].inputType == INPUT_IS_ARCHIVE)
    {
        printf("realInput = %s\n", p->args[curArg].realInput);
        use_subdir = 1;
    }
    curArg++;
  }
#endif

  /* If we're using a sub directory, create it... */
  if (use_subdir){
    Cmdline_t cp = {0};
    addArg(&cp, "mkdir .tmp");
    runCmd(p,&cp);
  }

  /* build ar command-line */
  addArg(&c,AR_ADD_WITH_REPLACE);
  addArg(&c,archiveName);
  addArg(&c," ");
  curArg = 1;
  while (curArg < p->numArgs)
  {
    switch (p->args[curArg].inputType)
    {
      /* BeOS Note.
       * Now that we're not using the firstInput flag we should really
       * skip the options as we only need to pass files into ar on BeOS.
       */
#if !BEOS_BUILD
      case INPUT_IS_OPTION:
        if (strstr(p->args[curArg].s,"-l")){
          addArg(&c,p->args[curArg].s);
          addArg(&c," ");
        }
        break;
#endif
      case INPUT_IS_LARCHIVE:
        if (!p->fromShlibtool) {
          if (!isLarchiveStatic(&(p->args[curArg].arch))) {
            _addSharedArg(&c, &(p->args[curArg].arch), p);
            break;
          } else 
            p->args[curArg].inputType = INPUT_IS_ARCHIVE;   
        }    
        break;        
      case INPUT_IS_ARCHIVE:
        /* OK, so if it's an archive, we expand it into the
         * temp directory, if it's just an object we copy it into
         * the directory.  We link against all .o's we find there,
         * so this may need looking at.
         */
        if (!p->fromShlibtool) {
          addArg(&cmd,"cd .tmp;ar x ../"); 
          addArg(&cmd, p->args[curArg].realInput);
          addArg(&cmd, ";cd ..");
          runCmd(p, &cmd);
        }
        break;        
      case INPUT_IS_OBJ:
      case INPUT_IS_LOBJ:
        assert(larch.numInputs < sizeof larch.inputs / sizeof larch.inputs[0]);
        larch.inputs[larch.numInputs] = strdup(p->args[curArg].realInput);
        ++larch.numInputs;

        if (! use_subdir){
          addArg(&c,p->args[curArg].realInput);
          addArg(&c," ");
        } else {
/* This shoulodn't really be needed here...
          if (p->args[curArg].realInput != NULL){
*/
            addArg(&cmd, "cp ");
            addArg(&cmd, p->args[curArg].realInput);
            addArg(&cmd, " .tmp");
            runCmd(p, &cmd);
/*
          }
*/
        }
        break;
      default:
        break;
    }
    ++curArg;
  }

  if (use_subdir)
    addArgUnescaped(&c, ".tmp/*.o");
    
  rc = runCmd(p,&c);

  if (use_subdir){
    Cmdline_t rmv = {0};
    addArg(&rmv, "rm -rf .tmp");
    runCmd(p, &rmv);
  }
  
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
    larch.staticLib = strdup(archiveName);
    larch.installPath = strdup(p->rpath);
    writeLarchive(&larch);
  }

  return rc;
}

static int parseCmdline(int argc,char **argv,Parms_t *p)
{
  int rc = 0;
  int curArg;
  const char *modeStr;
  enum {NORM, TARGET} state = NORM;

  p->mode = 0; /* not a valid mode */
  curArg = 1;
  while (curArg < argc)
  {
    p->args[curArg].inputType = NOT_INPUT;
    
    if (debug >= DEBUG_GORY_DETAILS)
      fprintf(debugf,"arg %d: %s\n",curArg,argv[curArg]);
   
#if SUPPORT_DLL_SPLIT
    p->bldSharedObj = 1;
#endif
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
    else if (!memcmp(argv[curArg],"--main=",7))
    {
#if SUPPORT_DLL_SPLIT
      p->main_obj = strdup(argv[curArg] + 7);
#else
      fprintf(stderr,
              "--main=foo is not supported on this platform.\n");
      exit(1);
#endif
    }
    else if (!memcmp(argv[curArg],"--core-dll=",11))
    {
#if SUPPORT_DLL_SPLIT
      p->core_dll = strdup(argv[curArg] + 11);
#else
      fprintf(stderr,
              "--core-dll=foo is not supported on this platform.\n");
      exit(1);
#endif
    }
    else if (!strcmp(argv[curArg],"-rpath"))
    {
      ++curArg;
      p->rpath = argv[curArg];
      /* david - 23 Apr 2001
       * This may be bogus, but we only set -rpath if we're
       * building shared libraries...
       *
       * 
       * Jeff: Yes, this is bogus :)  But if "-rpath" is always
       * added to the libtool command-line when the compiled
       * object will be build into a shared library then fair
       * enough; we can just rename the fromShlibtool flag to
       * bldSharedObj or similar.
       *
       * Jeff: Oops, -rpath is added when libmm.la is built.
       * We don't want a .so from that when doing a static
       * build.  I gotta comment this out for now.
       *
       * David: why is MM being built using -rpath?  That seems wrong if
       *        we're aiming for a static build.
       */
#if BEOS_BUILD /* keep from messing up BeOS...  but fix the mm build first */
       p->fromShlibtool = 1;
#endif
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
    else if (!strcmp(argv[curArg],"-static"))
    {
      p->linkStatic = 1;
    }
    else if (!strcmp(argv[curArg],"-no-install"))
    {
      fprintf(stderr,"warning: -no-install option ignored\n");
    }
    else
    {
      if (debug >= DEBUG_GORY_DETAILS)
        fprintf(debugf,"normal compile option: %s\n",argv[curArg]);
      assert(p->numArgs < MAX_ARGS);
      p->args[p->numArgs].s = argv[curArg];
      ++p->numArgs;

      if (!strcmp(argv[curArg],"-o"))
      {
        /* Whenever we see a -o the next argument is the target.
         * Also we default to building executables.  We can't guarentee
         * what extentions will be used for an executable object as this is
         * unix, so we just assume it.
         * Additionally, if we have more than one -o set, then  it's not
         * an error, stupid, but not an error.  We'll just accept this and
         * set our target as the final target passed in.
         */
        size_t targetLen = strlen(argv[curArg + 1]);
        curArg++;
        p->args[p->numArgs].s = argv[curArg];      
        ++p->numArgs;        
        p->target = argv[curArg];

  /* David: You had a comment here about this limiting what
   * we can build, but I'm not sure if that applied to my
   * version or your version...
   *
   * Jeff:  If this to be more generic then we should be able to build
   * any extension.  Look at the GNU libtool demo and test code to see
   * what I mean.  Basically they build .static versions, which we need
   * to treat as executable :)
   */

        if (!strcmp(p->target + targetLen - 3,".la"))
        {
          p->outputType = OUTPUT_IS_ARCHIVE;
#if SUPPORT_DLL_SPLIT 
        p->fromShlibtool = 1; 
#endif
        }
        else if (!strcmp(p->target + targetLen - 3,".lo"))
        {
          p->outputType = OUTPUT_IS_OBJ;
          p->args[p->numArgs - 2].inputType = INPUT_IS_OPTION;
        }
        else
          p->outputType = OUTPUT_IS_EXE;

        p->args[p->numArgs - 1].inputType = INPUT_IS_TARGET;       
      } else {
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
          readLarchive(&(p->args[p->numArgs - 1].arch), tmp, 0);
          /* if we have a static object then enter it as our realInput, but
           * if it's a shared object only, leave it empty.
           */
          if (isLarchiveStatic(&(p->args[p->numArgs - 1].arch))){         
            _getLarchiveStatic(tmp, &(p->args[p->numArgs - 1].arch));
            p->args[p->numArgs - 1].realInput = tmp;
          }
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
        else if (strstr(argv[curArg],".c") || strstr(argv[curArg],".cc"))
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
    }
    ++curArg;
  }

  if (p->mode == 0 &&
      p->showVersion)
  {
    p->mode = SHOWVERSION;
  }
 
  return rc;
}

static int link(Parms_t *p)
{
  int rc;

  switch(p->outputType)
  {
    case OUTPUT_IS_ARCHIVE:
#if SUPPORT_DLL_SPLIT
      rc = buildArchive(p);
      if (!p->linkStatic) { 
        rc = shlibtoolLink(p);
      }
#else
      if (p->fromShlibtool)
        rc = shlibtoolLink(p);
      else
        rc = buildArchive(p);
#endif
      break;
    case OUTPUT_IS_EXE:
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

  printf(PGM ": This is libtool 1.3.8 for " PLATFORM ".\n"
         "It acts enough like GNU libtool to allow Apache to be built.\n");
  return 0;
}

static void _buildCPcommand(Cmdline_t *c, const char * a1, const char *a2)
{
  memset(c,0,sizeof(Cmdline_t));
  addArg(c, "cp ");
  addArg(c, a1);
  addArg(c, " ");
  addArg(c, a2);
}

static int install2(Parms_t *p)
{
  int curArg;
  int rc = 0;
  Cmdline_t cmdline = {0};

  curArg = 0;
  while (curArg < p->numArgs)
  {
    addArg(&cmdline,p->args[curArg].s);
    addArg(&cmdline," ");
    ++curArg;
  }
  rc = runCmd(p,&cmdline);
  return rc;
}

static void updateFnameForInstall(Larchive_t *la)
{
  char *dotPos;
  /*
   * note: when called to install a library like
   *            libtoolexe --mode=install libapr.la /my/path/to/libs
   *       the sharedLib field may not be set and we'll need to use the
   *       fname field as the library name
   */
  char *old_sharedLib = la->sharedLib;

  if (!la->sharedLib) {
    la->sharedLib = la->fname;
  }
  /* a little bigger than necessary, but who wants to bother omitting the bytes for
   * the file extension of la->sharedLib?
   */
  la->fname = (char *)malloc(
    strlen(la->installPath) + strlen("/") + strlen(la->sharedLib) + strlen(".la") + 1);
  strcpy(la->fname, la->installPath);
  if (la->fname[strlen(la->fname) - 1] != '/') {
    strcat(la->fname, "/");
  }
  strcat(la->fname, la->sharedLib);
  /* now, replace file extension of path we just built with ".la" */
  dotPos = strrchr(la->fname, '.');
  if (!dotPos) {
    fprintf(stderr, "about to die; can't find dot in '%s'\n", la->fname);
    dumpLarchive(la); 
    assert(dotPos);
  }
  strcpy(dotPos, ".la");
  if (debug >= DEBUG_GORY_DETAILS)
    dumpLarchive(la);
  la->sharedLib = old_sharedLib;
}

static int install(Parms_t *p)
{
  int rc = 0;
  int cur;
  Cmdline_t c = {0};
  char *so;
  Larchive_t la;

  if (p->numArgs != 3 ||
      strcmp(p->args[0].s,"cp")) 
  {
    return install2(p);
  }
  assert(p->numArgs == 3);
  assert(!strcmp(p->args[0].s,"cp"));

  /* If it's a libtool archive, 
   *  read it in
   *  modify it
   *  write it out
   */
  if (p->args[1].inputType == INPUT_IS_LARCHIVE) {
    readLarchive(&la, p->args[1].s, 1);
    la.installPath = strdup(p->args[2].s);
    la.installed = 1;
    updateFnameForInstall(&la);
    writeLarchive(&la);
  }
#if SUPPORT_DLL_SPLIT
  if (la.sharedLib )             
#else
  if (p->fromShlibtool)
#endif  
  {
    char *so;

    so = strdup(p->args[1].s);
    strcpy(strstr(so,".la"),".so");

    _buildCPcommand(&c, so, p->args[2].s);
    rc = runCmd(p,&c);
  }
  /*else  need tweak for non-390?*/
  if (p->linkStatic)  
  {
    char *a;

    /* not building a dll; copy the foo.la and .libs/foo.a to
     * the target directory
     */

    _buildCPcommand(&c, p->args[1].s, p->args[2].s);
    rc = runCmd(p,&c);
      
    if (p->args[1].inputType == INPUT_IS_LARCHIVE){
      if (la.staticLib && la.sharedLib){
        _buildCPcommand(&c, la.sharedLib, p->args[2].s);
        runCmd(p,&c);
        _buildCPcommand(&c, la.staticLib, p->args[2].s);
        runCmd(p,&c);

      } else if (la.staticLib){
        _buildCPcommand(&c, la.staticLib, p->args[2].s);
        runCmd(p,&c);
      }else{
        _buildCPcommand(&c, la.sharedLib, p->args[2].s);
        runCmd(p,&c);
      }
    }        
  }
  
  return rc;
}

int main(int argc,char **argv)
{
  int rc;
  Parms_t parms = {0};

  debugf = stdout;
  debug = getenv("LIBTOOL_DEBUG") != NULL;
  if (debug)
    debug = atoi(getenv("LIBTOOL_DEBUG"));

  if (argc == 1)
  {
    fprintf(stderr,
            PGM ": Please run " PLATFORM " libtool with some parameters!\n");
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

  if (rc)
    fprintf(stderr,PGM ": returning error code %d...\n",rc);

  return rc;
}
