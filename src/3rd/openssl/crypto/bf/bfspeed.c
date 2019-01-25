
#if !defined(OPENSSL_SYS_MSDOS) && (!defined(OPENSSL_SYS_VMS) || defined(__DECC)) && !defined(OPENSSL_SYS_MACOSX)
# define TIMES
#endif

#include <stdio.h>

#include <openssl/e_os2.h>
#include OPENSSL_UNISTD_IO
OPENSSL_DECLARE_EXIT
#ifndef OPENSSL_SYS_NETWARE
# include <signal.h>
#endif
#ifndef _IRIX
# include <time.h>
#endif
#ifdef TIMES
# include <sys/types.h>
# include <sys/times.h>
#endif
    /*
     * Depending on the VMS version, the tms structure is perhaps defined.
     * The __TMS macro will show if it was.  If it wasn't defined, we should
     * undefine TIMES, since that tells the rest of the program how things
     * should be handled.  -- Richard Levitte
     */
#if defined(OPENSSL_SYS_VMS_DECC) && !defined(__TMS)
# undef TIMES
#endif
#ifndef TIMES
# include <sys/timeb.h>
#endif
#if defined(sun) || defined(__ultrix)
# define _POSIX_SOURCE
# include <limits.h>
# include <sys/param.h>
#endif
#include <openssl/blowfish.h>
/* The following if from times(3) man page.  It may need to be changed */
#ifndef HZ
# ifndef CLK_TCK
#  define HZ      100.0
# else                          /* CLK_TCK */
#  define HZ ((double)CLK_TCK)
# endif
#endif
#define BUFSIZE ((long)1024)
long run = 0;

double Time_F(int s);
#ifdef SIGALRM
# if defined(__STDC__) || defined(sgi) || defined(_AIX)
#  define SIGRETTYPE void
# else
#  define SIGRETTYPE int
# endif

SIGRETTYPE sig_done(int sig);
SIGRETTYPE sig_done(int sig)
{
    signal(SIGALRM, sig_done);
    run = 0;
# ifdef LINT
    sig = sig;
# endif
}
#endif

#define START   0
#define STOP    1

double Time_F(int s)
{
    double ret;
#ifdef TIMES
    static struct tms tstart, tend;

    if (s == START) {
        times(&tstart);
        return (0);
    } else {
        times(&tend);
        ret = ((double)(tend.tms_utime - tstart.tms_utime)) / HZ;
        return ((ret == 0.0) ? 1e-6 : ret);
    }
#else                           /* !times() */
    static struct timeb tstart, tend;
    long i;

    if (s == START) {
        ftime(&tstart);
        return (0);
    } else {
        ftime(&tend);
        i = (long)tend.millitm - (long)tstart.millitm;
        ret = ((double)(tend.time - tstart.time)) + ((double)i) / 1e3;
        return ((ret == 0.0) ? 1e-6 : ret);
    }
#endif
}

int main(int argc, char **argv)
{
    long count;
    static unsigned char buf[BUFSIZE];
    static unsigned char key[] = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    };
    BF_KEY sch;
    double a, b, c, d;
#ifndef SIGALRM
    long ca, cb, cc;
#endif

#ifndef TIMES
    printf("To get the most accurate results, try to run this\n");
    printf("program when this computer is idle.\n");
#endif

#ifndef SIGALRM
    printf("First we calculate the approximate speed ...\n");
    BF_set_key(&sch, 16, key);
    count = 10;
    do {
        long i;
        BF_LONG data[2];

        count *= 2;
        Time_F(START);
        for (i = count; i; i--)
            BF_encrypt(data, &sch);
        d = Time_F(STOP);
    } while (d < 3.0);
    ca = count / 512;
    cb = count;
    cc = count * 8 / BUFSIZE + 1;
    printf("Doing BF_set_key %ld times\n", ca);
# define COND(d) (count != (d))
# define COUNT(d) (d)
#else
# define COND(c) (run)
# define COUNT(d) (count)
    signal(SIGALRM, sig_done);
    printf("Doing BF_set_key for 10 seconds\n");
    alarm(10);
#endif

    Time_F(START);
    for (count = 0, run = 1; COND(ca); count += 4) {
        BF_set_key(&sch, 16, key);
        BF_set_key(&sch, 16, key);
        BF_set_key(&sch, 16, key);
        BF_set_key(&sch, 16, key);
    }
    d = Time_F(STOP);
    printf("%ld BF_set_key's in %.2f seconds\n", count, d);
    a = ((double)COUNT(ca)) / d;

#ifdef SIGALRM
    printf("Doing BF_encrypt's for 10 seconds\n");
    alarm(10);
#else
    printf("Doing BF_encrypt %ld times\n", cb);
#endif
    Time_F(START);
    for (count = 0, run = 1; COND(cb); count += 4) {
        BF_LONG data[2];

        BF_encrypt(data, &sch);
        BF_encrypt(data, &sch);
        BF_encrypt(data, &sch);
        BF_encrypt(data, &sch);
    }
    d = Time_F(STOP);
    printf("%ld BF_encrypt's in %.2f second\n", count, d);
    b = ((double)COUNT(cb) * 8) / d;

#ifdef SIGALRM
    printf("Doing BF_cbc_encrypt on %ld byte blocks for 10 seconds\n",
           BUFSIZE);
    alarm(10);
#else
    printf("Doing BF_cbc_encrypt %ld times on %ld byte blocks\n", cc,
           BUFSIZE);
#endif
    Time_F(START);
    for (count = 0, run = 1; COND(cc); count++)
        BF_cbc_encrypt(buf, buf, BUFSIZE, &sch, &(key[0]), BF_ENCRYPT);
    d = Time_F(STOP);
    printf("%ld BF_cbc_encrypt's of %ld byte blocks in %.2f second\n",
           count, BUFSIZE, d);
    c = ((double)COUNT(cc) * BUFSIZE) / d;

    printf("Blowfish set_key       per sec = %12.3f (%9.3fuS)\n", a,
           1.0e6 / a);
    printf("Blowfish raw ecb bytes per sec = %12.3f (%9.3fuS)\n", b,
           8.0e6 / b);
    printf("Blowfish cbc     bytes per sec = %12.3f (%9.3fuS)\n", c,
           8.0e6 / c);
    exit(0);
#if defined(LINT) || defined(OPENSSL_SYS_MSDOS)
    return (0);
#endif
}