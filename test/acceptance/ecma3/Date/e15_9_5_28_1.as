/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
    var SECTION = "15.9.5.28-1";
    var VERSION = "ECMA_1";
    startTest();

    writeHeaderToLog( SECTION + " Date.prototype.setMinutes(sec [,ms] )");

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    
    addNewTestCase( 0, 0, void 0, void 0,
                    "TDATE = new Date(0);(TDATE).setMinutes(0);TDATE",
                    UTCDateFromTime(SetMinutes(0,0,0,0)),
                    LocalDateFromTime(SetMinutes(0,0,0,0)) );

    addNewTestCase( 28800000, 59, 59, void 0,
                    "TDATE = new Date(28800000);(TDATE).setMinutes(59,59);TDATE",
                    UTCDateFromTime(SetMinutes(28800000,59,59)),
                    LocalDateFromTime(SetMinutes(28800000,59,59)) );

    addNewTestCase( 28800000, 59, 59, 999,
                    "TDATE = new Date(28800000);(TDATE).setMinutes(59,59,999);TDATE",
                    UTCDateFromTime(SetMinutes(28800000,59,59,999)),
                    LocalDateFromTime(SetMinutes(28800000,59,59,999)) );

    addNewTestCase( 28800000, 59, void 0, void 0,
                    "TDATE = new Date(28800000);(TDATE).setMinutes(59);TDATE",
                    UTCDateFromTime(SetMinutes(28800000,59,0)),
                    LocalDateFromTime(SetMinutes(28800000,59,0)) );

    addNewTestCase( 28800000, -480, void 0, void 0,
                    "TDATE = new Date(28800000);(TDATE).setMinutes(-480);TDATE",
                    UTCDateFromTime(SetMinutes(28800000,-480)),
                    LocalDateFromTime(SetMinutes(28800000,-480)) );

    addNewTestCase( 946684800000, 1234567, void 0, void 0,
                    "TDATE = new Date(946684800000);(TDATE).setMinutes(1234567);TDATE",
                    UTCDateFromTime(SetMinutes(946684800000,1234567)),
                    LocalDateFromTime(SetMinutes(946684800000,1234567)) );

    addNewTestCase( -2208988800000,59, 59, void 0,
                    "TDATE = new Date(-2208988800000);(TDATE).setMinutes(59,59);TDATE",
                    UTCDateFromTime(SetMinutes(-2208988800000,59,59)),
                    LocalDateFromTime(SetMinutes(-2208988800000,59,59)) );

    addNewTestCase( -2208988800000, 59, 59, 999,
                    "TDATE = new Date(-2208988800000);(TDATE).setMinutes(59,59,999);TDATE",
                    UTCDateFromTime(SetMinutes(-2208988800000,59,59,999)),
                    LocalDateFromTime(SetMinutes(-2208988800000,59,59,999)) );
/*
    addNewTestCase( "TDATE = new Date(-2208988800000);(TDATE).setUTCMilliseconds(123456789);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(-2208988800000,123456789)),
                    LocalDateFromTime(SetUTCMilliseconds(-2208988800000,123456789)) );

    addNewTestCase( "TDATE = new Date(-2208988800000);(TDATE).setUTCMilliseconds(123456);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(-2208988800000,123456)),
                    LocalDateFromTime(SetUTCMilliseconds(-2208988800000,123456)) );

    addNewTestCase( "TDATE = new Date(-2208988800000);(TDATE).setUTCMilliseconds(-123456);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(-2208988800000,-123456)),
                    LocalDateFromTime(SetUTCMilliseconds(-2208988800000,-123456)) );

    addNewTestCase( "TDATE = new Date(0);(TDATE).setUTCMilliseconds(-999);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(0,-999)),
                    LocalDateFromTime(SetUTCMilliseconds(0,-999)) );
*/

    
    function addNewTestCase( time, min, sec, ms, DateString, UTCDate, LocalDate) {
        var DateCase = new Date( time );
    
        if ( sec == void 0 ) {
            DateCase.setMinutes( min );
        } else {
            if ( ms == void 0 ) {
                DateCase.setMinutes( min, sec );
            } else {
                DateCase.setMinutes( min, sec, ms );
            }
        }
    
    
    //    fixed_year = ( ExpectDate.year >=1900 || ExpectDate.year < 2000 ) ? ExpectDate.year - 1900 : ExpectDate.year;
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             UTCDate.value,       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             UTCDate.value,       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      UTCDate.year,    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         UTCDate.month,  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          UTCDate.date,   DateCase.getUTCDate() );
    //    array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           UTCDate.day,    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         UTCDate.hours,  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       UTCDate.minutes,DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       UTCDate.seconds,DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  UTCDate.ms,     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         LocalDate.year,       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            LocalDate.month,      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             LocalDate.date,       DateCase.getDate() );
    //    array[item++] = new TestCase( SECTION, DateString+".getDay()",              LocalDate.day,        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            LocalDate.hours,      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          LocalDate.minutes,    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          LocalDate.seconds,    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     LocalDate.ms,         DateCase.getMilliseconds() );
    
        
         
                   /*    array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",
                                          DateCase.toString());*/
                         
    
    
        array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",
                                          (DateCase.myToString = Object.prototype.toString, DateCase.myToString()) );
    }
    return array;
}

function MyDate() {
    this.year = 0;
    this.month = 0;
    this.date = 0;
    this.hours = 0;
    this.minutes = 0;
    this.seconds = 0;
    this.ms = 0;
}
function LocalDateFromTime(t) {
    t = LocalTime(t);
    return ( MyDateFromTime(t) );
}
function UTCDateFromTime(t) {
 return ( MyDateFromTime(t) );
}
function MyDateFromTime( t ) {
    var d = new MyDate();
    d.year = YearFromTime(t);
    d.month = MonthFromTime(t);
    d.date = DateFromTime(t);
    d.hours = HourFromTime(t);
    d.minutes = MinFromTime(t);
    d.seconds = SecFromTime(t);
    d.ms = msFromTime(t);

    d.time = MakeTime( d.hours, d.minutes, d.seconds, d.ms );
    d.value = TimeClip( MakeDate( MakeDay( d.year, d.month, d.date ), d.time ) );
    d.day = WeekDay( d.value );

    return (d);
}

function SetMinutes( t, min, sec, ms ) {
    var TIME = LocalTime(t);
    var MIN =  Number(min);
    var SEC  = ( sec == void 0) ? SecFromTime(TIME) : Number(sec);
    var MS   = ( ms == void 0 ) ? msFromTime(TIME)  : Number(ms);
    var RESULT5 = MakeTime( HourFromTime( TIME ),
                            MIN,
                            SEC,
                            MS );
    return ( TimeClip(UTC( MakeDate(Day(TIME),RESULT5))) );
}
