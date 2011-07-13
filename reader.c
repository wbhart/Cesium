#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "reader.h"
#include "gc.h"

char * get_input(int * s, int * len, FILE * in)
{
   char buff[1000];
   char c;
   int done = 0;
   int i = 0;
   int parens = 0, braces = 0, bracks = 0;
   int in_comment = 0, in_string = 0;
   int stage = 0;

   while (!done)
   {
      while ((c = fgetc(in)) != '\n')
      {
         if (in_string)
         {
            if (c == '\\')
            {
               buff[i++] = '\\';
               c = fgetc(in);
            } else if (c == '"')
               in_string = 0;
         } else if (in_comment)
         {
            if (c == '*')
            {
               if ((c == fgetc(in)) == '/')
               {
                  buff[i++] = '*';
                  in_comment = 0;
               } else
               {
                  ungetc(c, in);
                  c = '*';
               }
            }
         } else
         {
            if (c == '/')
            {
               c = fgetc(in);
               if (c == '/')
               {
                  buff[i++] = '/';
                  while ((buff[i++] = fgetc(in)) != '\n')
                     ;
                  break;
               } else if (c == '*')
               {
                  buff[i++] = '/';
                  in_comment = 1;
               } else
               {
                  if (stage == 2)
                     stage = 3;
                  ungetc(c, in);
                  c = '/';
               }
            } else
            {
               if (stage == 2 && c != ' ' && c != '\t')
                  stage = 3;
               
               if (c == '(')
               {
                  parens++;
                  if (stage == 0)
                     stage = 1;
               }
               else if (c == ')')
               {
                  parens--;
                  if (stage == 1 && parens == 0)
                     stage = 2;
               }
               else if (c == '[')
                  bracks++;
               else if (c == ']')
                  bracks--;
               else if (c == '{')
                  braces++;
               else if (c == '}')
                  braces--;
               else if (c == '"')
                  in_string = 1;
            }
         }
            
         buff[i++] = c;
      }
      done = (bracks == 0 && parens == 0 && braces == 0 
          && !in_comment && !in_string);           
   }
   
   *s = stage;
   buff[i++] = '\0';
   char * str = GC_MALLOC(i);
   strcpy(str, buff);
   *len = i - 1;

   return str;
}
