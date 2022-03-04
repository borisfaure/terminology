#ifndef TERMINOLOGY_TY_COMMON_H_
#define TERMINOLOGY_TY_COMMON_H_ 1

int expect_running_in_terminology(void);
ssize_t ty_write(int fd, const void *buf, size_t count);

#define ON_NOT_RUNNING_IN_TERMINOLOGY_EXIT_1()                             \
  do                                                                       \
    {                                                                      \
       if (expect_running_in_terminology() != 0)                           \
         {                                                                 \
            fprintf(stderr, "not directly running in terminology\n");      \
            exit(1);                                                       \
         }                                                                 \
    }                                                                      \
  while (0)

#define HELP_ARGUMENT_DOC  "  -h or --help Display this help."
#define HELP_ARGUMENT_SHORT "[-h]"

#define ARGUMENT_ENTRY_CHECK(argc, argv, help_func) \
    do \
      { \
         int _i = 0; \
         for(_i = 0; _i < argc; _i++) \
           { \
             if (!strcmp(argv[_i], "--help") || !strcmp(argv[_i],"-h")) \
               { \
                  help_func(argv[0]); \
                  return EXIT_SUCCESS; \
               } \
         } \
      } \
    while(0)

#endif
