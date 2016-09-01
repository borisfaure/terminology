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

#define HELP_ARGUMENT_DOC  "  -h or --help Display this help."
#define HELP_ARGUMENT_SHORT "[-h]"

#define ARGUMENT_ENTRY_CHECK(argc, argv, help_func) \
    do \
      { \
         int i = 0; \
         for(i = 0; i < argc; i++) \
           { \
             if (!strcmp(argv[i], "--help") || !strcmp(argv[i],"-h")) \
               { \
                  help_func(argv[0]); \
                  return EXIT_SUCCESS; \
               } \
         } \
      } \
    while(0)

#endif
