/*
   Copyright (C) 2002 Vladimir Oleynik <dzo@simtreas.ru>
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


#ifdef TEST
extern void *xrealloc(void *p, size_t size);
#else
#include "libbb.h"      /* busybox source */
#endif


/* Exchange glibc vasprintf - minimize allocate memory version */
/* libc5 and uclibc have not vasprintf function */
void bb_vasprintf(char **string_ptr, const char *format, va_list args)
{
       int   bs = 128;
       char  stack_buff[128];
       char *buff = stack_buff;
       int   done;

       /* two or more loop, first - calculate memory size only */
       while(1) {
               done = vsnprintf (buff, bs, format, args);
/* Different libc have different interpretation vsnprintf returned value */
               if(done >= 0) {
                       if(done < bs && buff != stack_buff) {
                               /* allocated */
                               *string_ptr = buff;
                               return;
                       } else {
                               /* true calculate memory size */
                               bs = done+1;
                       }
               } else {
                       /*
                        * Old libc. Incrementaly test.
                        * Exact not minimize allocate memory.
                        */
                       bs += 128;
               }
               buff = xrealloc((buff == stack_buff ? NULL : buff), bs);
       }
}

void bb_asprintf(char **string_ptr, const char *format, ...)
{
       va_list p;

       va_start(p, format);
       bb_vasprintf(string_ptr, format, p);
       va_end(p);
}

#ifdef TEST
int main(int argc, char **argv)
{
       char *out_buf;
       char big_buf[200];
       int i;

       bb_asprintf(&out_buf, "Hi!\nargc=%d argv[0]=%s\n", argc, argv[0]);
       printf(out_buf);
       free(out_buf);

       for(i=0; i < sizeof(big_buf)-1; i++)
               big_buf[i]='x';
       big_buf[i]=0;
       bb_asprintf(&out_buf, "Test Big\n%s\n", big_buf);
       printf(out_buf);
       free(out_buf);

       return 0;
}

void *xrealloc(void *p, size_t size)
{
       void *p2 = realloc(p, size);
       if(p2==0) {
               fprintf(stderr, "TEST: memory_exhausted\n");
               exit(1);
       }
       return p2;
}
#endif
