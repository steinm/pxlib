extern void put_long_le(char *cp, long lval);
extern long get_long_le(char *cp);
extern short int get_short_le(char *cp);
extern void put_short_le(char *cp, short int sval);
extern void put_double_le(char *cp, double fval);
extern double get_double_le(char *cp);

extern void put_long_be(char *cp, long lval);
extern long get_long_be(char *cp);
extern short int get_short_be(char *cp);
extern void put_short_be(char *cp, short int sval);
extern void put_double_be(char *cp, double fval);
extern double get_double_be(char *cp);

extern void copy_fill(char *dp, char *sp, int len);
extern void copy_crimp(char *dp, char *sp, int len);
extern void px_set_date(char *cp, int year, int month, int day);
extern int px_date_year(char *cp);
extern int px_date_month(char *cp);
extern int px_date_day(char *cp);
extern char *px_cur_date(char *cp);
extern int px_get_date(char *cp);
extern void hex_dump(FILE *outfp, char *p, int len);

