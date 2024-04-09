/* Testing SQLCancel() */

#include "common.h"

#include <signal.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <freetds/thread.h>
#include <freetds/utils.h>
#include <freetds/bool.h>
#include <freetds/replacements.h>

#include <freetds/test_assert.h>

#if TDS_HAVE_MUTEX

#ifdef _WIN32
#undef HAVE_ALARM
#endif

static SQLTCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
static tds_mutex mtx;

static void
getErrorInfo(SQLSMALLINT sqlhdltype, SQLHANDLE sqlhandle)
{
	SQLINTEGER naterror = 0;
	SQLTCHAR msgtext[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT msgtextl = 0;

	msgtext[0] = 0;
	SQLGetDiagRec(sqlhdltype,
			      (SQLHANDLE) sqlhandle,
			      1,
			      sqlstate,
			      &naterror,
			      msgtext, (SQLSMALLINT) TDS_VECTOR_SIZE(msgtext), &msgtextl);
	sqlstate[TDS_VECTOR_SIZE(sqlstate)-1] = 0;
	fprintf(stderr, "Diagnostic info:\n");
	fprintf(stderr, "  SQL State: %s\n", C(sqlstate));
	fprintf(stderr, "  SQL code : %d\n", (int) naterror);
	fprintf(stderr, "  Message  : %s\n", C(msgtext));
}

static void
exit_forced(int s)
{
	exit(1);
}

#if HAVE_ALARM
static void
sigalrm_handler(int s)
{
	printf(">>>> SQLCancel() ...\n");
	CHKCancel("S");
	printf(">>>> ... SQLCancel done\n");

	alarm(4);
	signal(SIGALRM, exit_forced);
}
#else
#define alarm(x) return;
#define signal(sig,h)
#endif

#ifdef _WIN32

static HANDLE alarm_cond = NULL;

static DWORD WINAPI alarm_thread_proc(LPVOID arg)
{
	unsigned int timeout = (uintptr_t) arg;
	switch (WaitForSingleObject(alarm_cond, timeout * 1000)) {
	case WAIT_OBJECT_0:
		return 0;
	}
	abort();
	return 0;
}

#undef alarm
#define alarm tds_alarm
static void alarm(unsigned int timeout)
{
	static HANDLE thread = NULL;

	/* create an event to stop the alarm thread */
	if (alarm_cond == NULL) {
		alarm_cond = CreateEvent(NULL, TRUE, FALSE, NULL);
		assert(alarm_cond != NULL);
	}

	/* stop old alarm */
	if (thread) {
		SetEvent(alarm_cond);
		assert(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
		CloseHandle(thread);
		thread = NULL;
	}

	if (timeout) {
		ResetEvent(alarm_cond);

		/* start alarm thread */
		thread = CreateThread(NULL, 0, alarm_thread_proc, (LPVOID) (uintptr_t) timeout, 0, NULL);
		assert(thread);
	}
}
#endif

volatile bool exit_thread;

static TDS_THREAD_PROC_DECLARE(wait_thread_proc, arg)
{
	int n;

	tds_sleep_s(4);

	printf(">>>> SQLCancel() ...\n");
	CHKCancel("S");
	printf(">>>> ... SQLCancel done\n");

	for (n = 0; n < 4; ++n) {
		tds_sleep_s(1);
		tds_mutex_lock(&mtx);
		if (exit_thread) {
			tds_mutex_unlock(&mtx);
			return TDS_THREAD_RESULT(0);
		}
		tds_mutex_unlock(&mtx);
	}

	exit_forced(0);
	return TDS_THREAD_RESULT(0);
}

static void
Test(bool use_threads, bool return_data)
{
	tds_thread wait_thread;

#if !HAVE_ALARM
	if (!use_threads) return;
#endif

	printf("testing with %s\n", use_threads ? "threads" : "signals");
	printf(">> Wait 5 minutes...\n");
	if (!use_threads) {
		alarm(4);
		signal(SIGALRM, sigalrm_handler);
	} else {
		int err;

		exit_thread = false;
		alarm(120);
		err = tds_thread_create(&wait_thread, wait_thread_proc, NULL);
		if (err != 0) {
			perror("tds_thread_create");
			exit(1);
		}
	}
	if (!return_data)
		CHKExecDirect(T("WAITFOR DELAY '000:05:00'"), SQL_NTS, "E");
	else
		odbc_command2("SELECT MAX(p1.k + p2.k * p3.k ^ p4.k) FROM tab1 p1, tab1 p2, tab1 p3, tab1 p4", "E");

	tds_mutex_lock(&mtx);
	exit_thread = true;
	tds_mutex_unlock(&mtx);
	alarm(0);
	if (use_threads)
		tds_thread_join(wait_thread, NULL);

	getErrorInfo(SQL_HANDLE_STMT, odbc_stmt);
	if (strcmp(C(sqlstate), "HY008") != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		odbc_disconnect();
		exit(1);
	}

	odbc_reset_statement();

	odbc_command("SELECT name FROM sysobjects WHERE 0=1");
}

int
main(int argc, char **argv)
{
	if (tds_mutex_init(&mtx))
		return 1;

	if (odbc_read_login_info())
		exit(1);

	/*
	 * prepare our odbcinst.ini
	 * is better to do it before connect cause unixODBC cache INIs
	 * the name must be odbcinst.ini cause unixODBC accept only this name
	 */
	if (odbc_driver[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\nThreading = 0\n", odbc_driver);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}

	odbc_use_version3 = 1;
	odbc_connect();

	odbc_command("IF OBJECT_ID('tab1') IS NOT NULL DROP TABLE tab1");
	odbc_command("CREATE TABLE tab1 ( k INT, vc VARCHAR(200) )");

	printf(">> Creating tab1...\n");
	odbc_command("DECLARE @i INT\n"
		"SET @i = 1\n"
		"WHILE @i <= 2000 BEGIN\n"
		"INSERT INTO tab1 VALUES ( @i, 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' )\n"
		"SET @i = @i + 1\n"
		"END");
	while (CHKMoreResults("SNo") == SQL_SUCCESS)
		continue;
	printf(">> ...done.\n");

	odbc_reset_statement();

	Test(false, false);
	Test(true,  false);
	Test(false, true);
	Test(true,  true);

	odbc_command("DROP TABLE tab1");

	odbc_disconnect();
	return 0;
}

#else
int
main(void)
{
	printf("Not possible for this platform.\n");
	return 0;
}
#endif

