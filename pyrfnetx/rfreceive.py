"""
Listen for sensors one or more channels and store received messages.
"""

import re
import sqlite3
import io
import time
import subprocess
import logging
from ctypes import c_ubyte

import serial
from smbus2 import SMBusWrapper

BUSNUM = 1
DEVICE_ADDR = 0x10
I2C_READ_DELAY = 0.005
I2C_WRITE_DELAY = 0.075
LOOP_DELAY = 0.1

logging.basicConfig()
log = logging.getLogger()
log.setLevel(logging.INFO)

class I2CError(Exception):
    pass

class ChecksumError(Exception):
    pass

def i2c_write_int(busnum, address, number):
    time.sleep(I2C_WRITE_DELAY)
    try:
        assert number<=255
        with SMBusWrapper(busnum) as bus:
            bus.write_byte(address, c_ubyte(number))

        return True

    except IOError:
        log.error('Error writing to I2C device: {:d}, value: {}'.format(
                address, number))
        raise I2CError()

    except:
        raise

def i2c_read_array(busnum, address, size):
    time.sleep(I2C_READ_DELAY)
    with SMBusWrapper(busnum) as bus:
        arr = bus.read_i2c_block_data(address, 0, size)

    return arr

def i2c_read_string(busnum, address, nbytes):
    """
    Return character data read from I2C.
    """
    buff = []
    try:
        with SMBusWrapper(busnum) as bus:
            for i in range(nbytes+2):
                time.sleep(I2C_READ_DELAY)
                c = bus.read_byte(address)
                buff.append(c)

    except IOError:
        log.debug('Error reading string. device: {:d}'.format(address))
        raise I2CError()

    except:
        raise

    log.debug(buff)

    buff_checksum = buff[-2]<<8 | buff[-1]
    local_checksum = sum(buff[:-2])
    if buff_checksum!=local_checksum:
        log.error('Checksums do not match, buff: {}, local: {}'.format(
                buff_checksum, local_checksum))
        raise ChecksumError()

    s = ''.join([chr(v) for v in buff[:-2]])
    return s

def i2c_read_int(busnum, addr):
    try:
        with SMBusWrapper(busnum) as bus:
            time.sleep(I2C_READ_DELAY)
            data = bus.read_byte(addr)
            return int(data)

    except IOError:
        log.debug('Error reading int. device: :d{}'.format(addr))
        raise I2CError()

    except:
        raise

def listen_i2c(busnum, address, db):
    """
    """

    dbconn = sqlite3.connect(db)

    while True:
        time.sleep(LOOP_DELAY)
        try:
            if not i2c_write_int(busnum, address, 1):
                continue
            log.debug('Sent - 1')

            nbytes = i2c_read_int(busnum, address)
            if not nbytes:
                continue

            elif not nbytes>0:
                log.debug('Nothing to read - {}'.format(nbytes))
                continue

            # Signal the next transmit should be the message
            log.debug('Signal ready to read.')
            if not (i2c_write_int(busnum, address, 2)):
                continue

            log.debug('Read {} bytes'.format(nbytes))
            nretrys = 0
            while 1:
                try:
                    msg = i2c_read_string(busnum, address, nbytes)

                except (I2CError, ChecksumError):
                    msg = None

                except:
                    raise

                if not msg is None:
                    break
                    #continue

                if nretrys<5:
                    log.debug('Retry read - {}'.format(nretrys))
                    # Signal to restart the current message
                    i2c_write_int(busnum, address, 4)
                    nretrys += 1

                else:
                    log.warn('Error reading message.')
                    break

            # Signal to advance the message queue
            #bus.write_byte(address, 3)
            i2c_write_int(busnum, address, 3)

            log.info('Packet: {}'.format(msg))

            data = parse_packet(msg)
            if not data:
                log.info('Bad packet: {}'.format(msg))
                continue

            net_id = data['net_id']
            unit_id = get_unit_id(net_id, dbconn)

            m = 'Net ID: {}, Unit: {}, Msg: {}'
            log.debug(m.format(net_id, unit_id, msg))

            insert_message(msg, unit_id, dbconn)

        except (KeyboardInterrupt, SystemExit):
            print('Exiting!')
            dbconn.close()
            raise

        except I2CError:
            log.error('I2C Error')
            #i2c_write_int(busnum, address, 0)
            continue

        except:
            dbconn.close()
            raise

    # Note: We will never get here if all exceptions re-raise
    dbconn.close()

def parse_packet(packet):
    fld_xref = {
        'U':'net_id'
        , 'T':'temperature'
        , 'H':'humidity'
        , 'V':'volts'
        , 'C':'seq_num'
    }

    data = {}

    try:
        rx_seq, rx_time, packet = packet.split(',')
        data['rx_seq'] = rx_seq
        data['rx_time'] = rx_time

    except:
        raise IOError('Unexpected data structure. {}'.format(packet))

    flds = packet.split(';')
    for fld in flds:
        abv, value = fld.split(':')
        data[fld_xref[abv]] = value

#    unit_pat = re.compile('U:([0-9]{4});')
#    s = re.search(unit_pat, packet)
#    if not s:
#        return None
#
#    data['net_id'] = s.groups()[0]

    return data

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
        log.debug('Messages stored to DB, {}'.format(msg))

    except:
        log.error('Error inserting message {} for unit {}'.format(msg, unit_id))
        conn.rollback()

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
    db = '/home/pi/rfnetx/pyrfnetx/rfnetx_data.sqlite'
    init_db(db)

    listen_i2c(BUSNUM, DEVICE_ADDR, db)

