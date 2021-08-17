#ifndef CNOID_URDFBODYLOADER_EXPORTDECL_H
# define CNOID_URDFBODYLOADER_EXPORTDECL_H

# if defined _WIN32 || defined __CYGWIN__
#  define CNOID_URDFBODYLOADER_DLLIMPORT __declspec(dllimport)
#  define CNOID_URDFBODYLOADER_DLLEXPORT __declspec(dllexport)
#  define CNOID_URDFBODYLOADER_DLLLOCAL
# else
#  if __GNUC__ >= 4
#   define CNOID_URDFBODYLOADER_DLLIMPORT __attribute__ ((visibility("default")))
#   define CNOID_URDFBODYLOADER_DLLEXPORT __attribute__ ((visibility("default")))
#   define CNOID_URDFBODYLOADER_DLLLOCAL  __attribute__ ((visibility("hidden")))
#  else
#   define CNOID_URDFBODYLOADER_DLLIMPORT
#   define CNOID_URDFBODYLOADER_DLLEXPORT
#   define CNOID_URDFBODYLOADER_DLLLOCAL
#  endif
# endif

# ifdef CNOID_URDFBODYLOADER_STATIC
#  define CNOID_URDFBODYLOADER_DLLAPI
#  define CNOID_URDFBODYLOADER_LOCAL
# else
#  ifdef CnoidMediaPlugin_EXPORTS
#   define CNOID_URDFBODYLOADER_DLLAPI CNOID_URDFBODYLOADER_DLLEXPORT
#  else
#   define CNOID_URDFBODYLOADER_DLLAPI CNOID_URDFBODYLOADER_DLLIMPORT
#  endif
#  define CNOID_URDFBODYLOADER_LOCAL CNOID_URDFBODYLOADER_DLLLOCAL
# endif

#endif

#ifdef CNOID_EXPORT
# undef CNOID_EXPORT
#endif
#define CNOID_EXPORT CNOID_URDFBODYLOADER_DLLAPI