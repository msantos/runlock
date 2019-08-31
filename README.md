# SYNOPSIS

runlock [*options*] *command* *arg* *...*

# DESCRIPTION

`runlock` rate limits commands based on their exit status and a
duration.

Commands are forked and supervised by `runlock`, usually as part of a
cron job. `runlock` ensures these properties:

* only a single instance of a cron job can be running

* a hung cron job cannot exceed the scheduled cron job interval

* a cron job task can be retried multiple times

* if the task fails (exits non-0), it will be re-run

* if the task succeeds (exits 0), the task will not be re-run until the
  scheduled cron job interval has elapsed

## BUILDING

    make

    # using musl
    ./musl-make

# EXAMPLES

## Cronjob

As an example, assume `report-generator` is a program that connects to
a database, retrieves some data, performs some analysis then formats and
emails it. If the process fails at any point, the program exits with an
error code.

~~~
# Run report each month on the 25th at 9:07am
* 7 9 25 * * report-generator
~~~

If the cron host or database happens to be down during this time, report
generation will fail.

~~~
* 7 9,10,11,12,13,14,15,16,17 25,26,27,28,29,30,31 * * runlock --file=$HOME/.runlock/report-generator --timestamp="$(date --date="last month" +%s)" report-generator
~~~

The crontab schedules the report generation to be run hourly from 9:07am
to 5:07pm for the final days of the month. If the program succeeds,
subsequent runs do nothing. If the program fails, each subsequent run
will retry the operation until it succeeds.

# OPTIONS

-t, --timestamp *seconds*
: threshold timestamp in epoch seconds

-T, --timeout *seconds*
: command timeout

-s, --signal *sig*
: signal sent on timeout (default: SIGTERM)

-n, --dryrun
: do nothing

-P, --print
: print remaing time until next run

-f, --file
: timestamp lock file

-v, --verbose
: verbose mode
