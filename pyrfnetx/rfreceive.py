"""
Listen for sensors one or more channels and store received messages.
"""

import re
import sqlite3
import io
import time
import subprocess

import serial
from smbus2 import smbus2

BUSNUM = 1

def i2c_write_number(bus, address, number):
    try:
        bus.write_byte(address, number)
        return True

    except IOError:
        subprocess.call(['i2cdetect','-y',str(BUSNUM)])
        return False

def i2c_read_array(bus, address, size):
    try:
        arr = bus.read_i2c_block_data(address, 0, size)

    except IOError:
        subprocess.call(['i2cdetect','-y',str(BUSNUM)])
        return None

    return arr

def i2c_read_string(bus, address):
    """
    Return character data read from I2C.
    """
    s = ''
    nretrys = 0
    while 1:
        if nretrys>5:
            return None

        try:
            buff = bus.read_i2c_block_data(address, 0, 11)

        except IOError:
            raise
            subprocess.call(['i2cdetect','-y',str(BUSNUM)])
            # acknowledge the bad read
            i2c_write_number(bus, address, 0)
            nretrys += 1
            continue

        time.sleep(0.1)

        # acknowledge the buffer, good or bad
        print(buff[-1], sum(buff[:-1]))
        if buff[-1]==sum(buff[:-1]):
            i2c_write_number(bus, address, 1)
        else:
            i2c_write_number(bus, address, 0)
            nretrys += 1

        time.sleep(0.1)

        done = False
        for c in buff:
            if c==0x0:
                done = True
                break

            s += chr(c)

        if done:
            break

    return s

def listen_i2c(address, db):
    """
    """

    bus = smbus2.SMBus(BUSNUM)

    unit_pat = re.compile('U:([0-9]{4});')
    dbconn = sqlite3.connect(db)

    while True:
        try:
            r = i2c_write_number(bus, address, 1)
            if not r:
                print('Error writing I2C at {}'.format(address))

            time.sleep(0.5)

#            data = i2c_read_array(bus, address, 5)
#            if data is None:
#                print('Error reading I2C at {}'.format(address))
#                data = []
#
#            print('Data: {}'.format([chr(c) for c in data]))

            msg = i2c_read_string(bus, address)
            if msg is None:
                print('Error reading I2C at {}'.format(address))
                msg = '*ERROR'

            print('Telem: {}'.format(msg))

            time.sleep(0.5)

#            s = re.search(unit_pat, msg)
#            net_id = s.groups()[0]
#            unit_id = get_unit_id(net_id, dbconn)
#
#            print(net_id, unit_id, msg)
#            insert_message(msg, unit_id, dbconn)

        except (KeyboardInterrupt, SystemExit):
            print('Exiting!')
            dbconn.close()
            bus.close()
            raise

        except:
            bus.close()
            dbconn.close()
            raise

    # Note: We will never get here if all exceptions re-raise
    bus.close()
    dbconn.close()


# TODO: This needs to run in it's own thread or process.
def listen_serial(port, db, **kwargs):
    """
    Listen on a serial port for sensor data packets.

    Args:
        port: Serial port to listen on.
        db: SQLite database to post data to.

    Addtional keyword arguments are passed to serial.Serial for
    port configuration.
    """

    unit_pat = re.compile('U:([0-9]{4});')

    dbconn = sqlite3.connect(db)
    tty = serial.Serial(port
              , baudrate=9600
              , bytesize=serial.EIGHTBITS
              , parity=serial.PARITY_NONE
              , stopbits=serial.STOPBITS_ONE
              , timeout=1)
    sio = io.TextIOWrapper(io.BufferedRWPair(tty, tty), newline='\r\n')
    tty.reset_input_buffer()

    while True:
        try:
            msg = sio.readline().strip()
            # msg = sio.readline().decode('utf-8').strip()
#            s = re.search(unit_pat, msg)
            b = tty.read(size=2)
            print(b)
#            net_id = s.groups()[0]
#            unit_id = get_unit_id(net_id, dbconn)

#            print(net_id, unit_id, msg)
#            insert_message(msg, unit_id, dbconn)

        except (KeyboardInterrupt, SystemExit):
            print('Exiting!')
            dbconn.close()
            tty.close()
            raise

        except:
            dbconn.close()
            tty.close()
            raise

    # Note: We will never get here if all exceptions re-raise
    dbconn.close()
    tty.close()

def insert_message(msg, unit_id, conn):
    """
    Insert a message into the database.
    """
    sql = """
        insert into messages
            (unit_id, message_string)
        values('{unit_id}','{msg}')
        """
    try:
        conn.execute(sql.format(**locals()))

    except:
        print('Error inserting message {} for unit {}'.format(msg, unit_id))
        raise

    conn.commit()

def get_unit_id(net_id, conn):
    """
    Return the unit_id matching a net_id
    """

    sql = """select unit_id from units where net_id='{}'""".format(net_id)
    cur = conn.cursor()
    try:
        cur.execute(sql)
        unit_id = cur.fetchone()[0]

    except:
        sql = """insert into units (net_id) values('{}')""".format(net_id)
        cur.execute(sql)
        cur.close()
        conn.commit()
        unit_id = get_unit_id(net_id, conn)

    return unit_id

def init_db(path):
    """
    Initialize the database schema.
    """

    conn = sqlite3.connect(path)

    try:
        conn.execute('drop table messages')
        conn.execute('drop table units')

    except:
        pass

    units_sql = """
        create table units (
            unit_id text primary key default (lower(hex(randomblob(16))))
            , net_id varchar(10) not null
            , label varchar(30)
            , loc_name varchar(30)
            , loc_point varchar(100)
            )
        """
    conn.execute(units_sql)

    messages_sql = """
        create table messages (
            message_id text primary key default (lower(hex(randomblob(16))))
            , unit_id text not null
            , insert_timestamp real default CURRENT_TIMESTAMP
            , message_string text not null
            , foreign key (message_id)
                references units(unit_id)
                on delete cascade
            )
        """

    conn.execute(messages_sql)

    conn.close()

if __name__=='__main__':
    db = 'rfnetx_data.sqlite'
    init_db(db)

    listen_i2c(0x04, db)

