/*
 * TODO: parser
 * TODO: dependency resolver
 * TODO: executer
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define pm_skip 0
#define pm_target 1
#define pm_cmd 2
#define pm_def 3

#define debug 0
#define debg(...) if(debug) printf(__VA_ARGS__)

struct target {
	int isliteral; /* set to zero is the target contains '%', nonzero otherwise */
	char *name; /* name of target */
	char **deps; /* what is needed for this target to be build */
	char **cmds; /* what is run */
};

char *filebuf, ***vars=0;
unsigned int filebuflen;

struct target **targetlist=0;

void*
xmalloc(size_t size) {
	void *ret;
	if((ret=malloc(size)) == 0) {
		fprintf(stderr, "make: can't allocate %u bytes\n", size);
		exit(1);
	}
	return ret;
}

void*
xrealloc(void *ptr, size_t size) {
	void *ret;
	if((ret=realloc(ptr, size)) == 0) {
		fprintf(stderr, "make: can't reallocate %p to %u bytes", ptr, size);
		exit(1);
	}
	return ret;
}

int
readtobuf(int f) {
	unsigned int bufsize;
	int w, i;
	
	bufsize=1024;
	filebuf=xmalloc(bufsize);
	
	for(i=0; (w=read(f, filebuf+i, 1024)) && w!=-1; i+=w)
		if(i+w+1024>bufsize) {
			bufsize=((i+w+512)/1024+1)*1024; /* To ensue that there is always at least 1kB free in the buffer */
			filebuf=xrealloc(filebuf, bufsize);
		}
	
	if(w==-1) {
		free(filebuf);
		return -1;
	}
	
	i+=w;
	/* all lines should be terminated */
	if(filebuf[i-1]!='\n') {
		filebuf=xrealloc(filebuf, i+1);
		filebuf[i]='\n';
		i++; /* size was increased by one */
	}
	
	filebuflen=i;
	return 0;
}

void
deltarget(struct target *t) {
	struct target tmp;
	char **pp;
	
	memcpy(&tmp, t, sizeof(struct target));
	free(t);
	
	free(tmp.name);
	
	for(pp=tmp.deps; *pp; pp++)
		free(*pp);
	free(tmp.deps);
}

char*
preprocess(char *start, unsigned int line) {
	char *ret, *p, *varname, *varval;
	unsigned int i, di;
	
	ret=0; /* realloc should regard NULL as 0B allocation */
	for(p=start, di=0; *p; p++)
		if(*p=='$') {
			if(!*++p)
				break;
			
			if(*p!='(') {
				if(!isalnum(*p) && *p!='@' && *p!='<' && *p!='.' && *p!='_') {
					fprintf(stderr, "make: %u: illegal char in identifier: '%c'\n", line, *p);
					exit(1);
				}
				varname=xmalloc(2);
				varname[0]=*p;
				varname[1]=0;
			} else {
				p++;
				varname=0; /* see above */
				for(i=0; *p!=')'; p++) {
					if(!*p) {
						fprintf(stderr, "make: %u: missing ')'\n", line);
						exit(1);
					}
					if(!isalnum(*p) && *p!='@' && *p!='<' && *p!='.' && *p!='_') {
						fprintf(stderr, "make: %u: illegal char in identifier: '%c'\n", line, *p);
						exit(1);
					}
					varname=xrealloc(varname, i+1);
					varname[i++]=*p;
				}
				varname=xrealloc(varname, i+1);
				varname[i]=0;
			}
			
			if(!vars) {
				vars=xmalloc(sizeof(char**));
				vars[0]=0;
			}
			
			for(i=0; vars[i] && strcmp(vars[i][0], varname); i++);
			if(vars[i])
				varval=vars[i][0];
			else if(!(varval=getenv(varname))) { /* if getenv returns NULL, this gets run; if getenv returns a value, it ends in varval */
				varval="";
			}
			free(varname);
			
			ret=xrealloc(ret, di+strlen(varval));
			memcpy(ret+di, varval, strlen(varval));
			di+=strlen(varval);
		} else {
			ret=xrealloc(ret, di+1);
			ret[di++]=*p;
		}
	
	while(isblank(ret[di])) /* remove trailing whitespace */
		di--;
	di++;
	ret=xrealloc(ret, di+1);
	ret[di]=0;
	
	return ret;
}

void
parseline(char *start, unsigned int *mode, struct target *t, unsigned int line) {
	unsigned int i;
	char *name;
	/* TODO: implement variables, trimming of comments and removal of trailing whitespace in target/var declarations */
	debg("\tparseline\n");
	if(*start=='\t') {
		debg("\t\tcmd\n");
		if(*mode==pm_target || *mode==pm_cmd) {
			if(!(*t).cmds) { /* Set up cmds list if we don't already have it */
				debg("\t\t\tcreatecmds\n");
				(*t).cmds=xmalloc(sizeof(char*));
				(*t).cmds[0]=0;
			}
			
			debg("\t\t\tselect\n");
			/* debg
			for(i=0; (*t).cmds[i]; i++);
			debg */
			for(i=0; (*t).cmds[i]; i++) debg("%u->%p: %p\n", i, (*t).cmds+i, (*t).cmds[i]); /* debg */
			
			(*t).cmds[i]=preprocess(start+1, line);
			
			(*t).cmds=xrealloc((*t).cmds, i+2*sizeof(char*)); /* i+1 = current size -> i+2 = one bigger */
			(*t).cmds[i+1]=0;
		} else {
			fprintf(stderr, "make: %u: unexpected tab\n", line);
			exit(1);
		}
		*mode=pm_cmd;
	} else if(*mode==pm_cmd || *mode==pm_target) {
		debg("\t\tsavetarget\n");
		/* save target */
		if(!targetlist) {
			targetlist=xmalloc(sizeof(struct target*));
			targetlist[0]=0;
		}
		
		for(i=0; targetlist[i]; i++);
		
		targetlist[i]=xmalloc(sizeof(struct target));
		memcpy(targetlist[i], t, sizeof(struct target));
		targetlist=xrealloc(targetlist, i+2); /* see above */
		targetlist[i+1]=0;
		memset(t, 0, sizeof(struct target));
	}
	if(isalnum(*start) || *start=='.' || *start=='_' || *start=='%' || *start=='$') {
		debg("\t\tdecl\n");
		for(i=0; start[i]!=':' && start[i]!='=' && start[i]!='?' && start[i]!='+'; i++)
			if(!start[i]) {
				fprintf(stderr, "make: %u: missing ':', '=', '?=' or '+='\n", line);
				exit(1);
			}
		if(start[i]==':') {
			debg("\t\t\ttarget\n");
			name=xmalloc(i+1);
			memcpy(name, start, i);
			name[i]=0;
			name=preprocess(name, line);
			(*t).name=name;
			puts((*t).name); /* devout */
			/* TODO: deps */
			
			free(name);
			*mode=pm_target;
		}
		/* TODO: variables */
	} else if(!*start || *start=='#') {
		*mode=pm_skip;
	} else if(*start!='\t') {
		fprintf(stderr, "make: %u: cannot parse\n", line);
		exit(1);
	}
}

void
parse(void) {
	char *p;
	unsigned int line, mode;
	struct target t;
	
	/* Make it so that there is a nice null-terminator after every line. Yes, this will break if file contains null-bytes */
	for(p=filebuf; p-filebuf<filebuflen; p++)
		if(*p=='\n')
			*p=0;
	
	memset(&t, 0, sizeof(struct target));
	
	for(p=filebuf, line=1, mode=pm_skip; p-filebuf<filebuflen; p++) {
		parseline(p, &mode, &t, line);
		line++;
		while(*p)
			p++;
	}
}

int
main(int argc, char **argv) {
	int fd;
	
	/* TODO: arg handling */
	fd=0;
	
	if(readtobuf(fd))
		return -1;
	
	parse();
	return 0;
}
