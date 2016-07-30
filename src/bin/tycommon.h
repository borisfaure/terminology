#ifndef _TY_COMMON_H__
#define _TY_COMMON_H__ 1

Eina_Bool is_running_in_terminology(void);

#define ON_NOT_RUNNING_IN_TERMINOLOGY_EXIT_1()                             \
  do                                                                       \
    {                                                                      \
       if (!is_running_in_terminology())                                   \
         {                                                                 \
            fprintf(stderr, "not running in terminology\n");               \
            exit(1);                                                       \
         }                                                                 \
    }                                                                      \
  while (0)


#endif
