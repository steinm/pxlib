void put_long(char *cp, long lval);
extern long get_long(char *cp);
extern int get_short(char *cp);
void put_short(char *cp, int sval);
void put_double(char *cp, double fval);
extern double get_double(char *cp);
void copy_fill(char *dp, char *sp, int len);
void copy_crimp(char *dp, char *sp, int len);
void px_set_date(char *cp, int year, int month, int day);
extern int px_date_year(char *cp);
extern int px_date_month(char *cp);
extern int px_date_day(char *cp);
extern char *px_cur_date(char *cp);
int px_get_date(char *cp);
void hex_dump(char *p, int len);

