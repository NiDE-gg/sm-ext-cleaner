#if defined _cleaner_included
 #endinput
#endif
#define _cleaner_included

public Extension __ext_cleaner =
{
	name = "cleaner",
	file = "cleaner.ext",
#if defined AUTOLOAD_EXTENSIONS
	autoload = 1,
#else
	autoload = 0,
#endif
#if defined REQUIRE_EXTENSIONS
	required = 1,
#else
	required = 0,
#endif
};

#if !defined REQUIRE_EXTENSIONS
public __ext_cleaner_SetNTVOptional()
{
}
#endif
