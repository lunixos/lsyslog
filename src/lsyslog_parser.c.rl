#include <stdio.h>

%%{

    machine syslog;

    action return {
        printf("\nPUB syslog.%.*s.%s.%.*s %.*s\n", hostname_len, hostname, log_levels[log_level], tag_len, tag, message_len, message);
    }

    action debug {
        printf("%c", *p);
    }

    action init_hostname {
        hostname = p;
    }

    action copy_hostname {
        hostname_len += 1;
    }

    action init_tag {
        tag = p;
    }

    action copy_tag {
        tag_len += 1;
    }

    action init_message {
        message = p;
    }

    action copy_message {
        message_len += 1;
    }

    action level_emergency {
        log_level = 0;
    }

    action level_alert {
        log_level = 1;
    }

    action level_critical {
        log_level = 2;
    }

    action level_error {
        log_level = 3;
    }

    action level_warning {
        log_level = 4;
    }

    action level_notice {
        log_level = 5;
    }

    action level_info {
        log_level = 6;
    }

    action level_debug {
        log_level = 7;
    }

    priority = '<' (
        ( ( '0' | '8' | '16' | '24' | '32' | '40' | '48' | '56' | '64' | '72' | '80' | '88' | '96' | '104' | '112' | '120' | '128' | '136' | '144' | '152' | '160' | '168' | '176' ) '>' @level_emergency ) |
        ( ( '1' | '9' | '17' | '25' | '33' | '41' | '49' | '57' | '65' | '73' | '81' | '89' | '97' | '105' | '113' | '121' | '129' | '137' | '145' | '153' | '161' | '169' | '177' ) '>' @level_alert ) |
        ( ( '2' | '10' | '18' | '26' | '34' | '42' | '50' | '58' | '66' | '74' | '82' | '90' | '98' | '106' | '114' | '122' | '130' | '138' | '146' | '154' | '162' | '170' | '178' ) '>' @level_critical ) |
        ( ( '3' | '11' | '19' | '27' | '35' | '43' | '51' | '59' | '67' | '75' | '83' | '91' | '99' | '107' | '115' | '123' | '131' | '139' | '147' | '155' | '163' | '171' | '179' ) '>' @level_error ) |
        ( ( '4' | '12' | '20' | '28' | '36' | '44' | '52' | '60' | '68' | '76' | '84' | '92' | '100' | '108' | '116' | '124' | '132' | '140' | '148' | '156' | '164' | '172' | '180' ) '>' @level_warning ) |
        ( ( '5' | '13' | '21' | '29' | '37' | '45' | '53' | '61' | '69' | '77' | '85' | '93' | '101' | '109' | '117' | '125' | '133' | '141' | '149' | '157' | '165' | '173' | '181' ) '>' @level_notice ) |
        ( ( '6' | '14' | '22' | '30' | '38' | '46' | '54' | '62' | '70' | '78' | '86' | '94' | '102' | '110' | '118' | '126' | '134' | '142' | '150' | '158' | '166' | '174' | '182' ) '>' @level_info ) |
        ( ( '7' | '15' | '23' | '31' | '39' | '47' | '55' | '63' | '71' | '79' | '87' | '95' | '103' | '111' | '119' | '127' | '135' | '143' | '151' | '159' | '167' | '175' | '183' ) '>' @level_debug )
        );
    version = '1';
    timestamp = [0-9\-T:.+]+;
    hostname = [A-Za-z0-9\-]+;
    tag = (any - ' ')+;
    pid = ('-' | [0-9]+);
    msgid = '-';
    structured_data = '[' (any - ']')* ']';
    message = (any - 0)*;

    main := (priority $debug
            version
            ' '
            timestamp
            ' '
            hostname >init_hostname $copy_hostname
            ' '
            tag >init_tag $copy_tag
            ' '
            pid 
            ' '
            msgid
            ' '
            structured_data
            ' '
            message >init_message $copy_message $debug
            0 @return)
            $err{ fprintf(stderr, "failed to parse at %c, cs=%d\n", *p, fcurs); };


    write data;

}%%

int lsyslog_parser_parse (
    const char * const buf,
    const int buf_len
)
{
    const char * p = buf;
    const char * pe = buf + buf_len;
    const char * eof = 0;
    int cs = 0;

    int log_level = 0;
    const char * log_levels[] = {
        "emerg",
        "alert",
        "crit",
        "err",
        "warn",
        "notice",
        "info",
        "debug"
    };

    const char * hostname;
    int hostname_len = 0;

    const char * tag;
    int tag_len = 0;

    const char * message;
    int message_len = 0;

    %% write init;
    %% write exec;

    printf("ok\n");

}
