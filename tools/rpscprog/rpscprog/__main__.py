import binascii
import click
import logging
import serial
import time

# rpscprog: A CLI tool to expedite scalable programming of controllers 

def do_serial_command(ctx, opcode, data):
  """Opens the serial port and performs the requested special command"""
  logging.info("Opening serial port %s at baudrate %s", ctx.obj['PORT'], ctx.obj['BAUD'])
  cmd = bytearray(4)
  cmd[0] = 0x7e    # Renard protocol start byte
  cmd[1] = 0x7a    # Special / extention command
  cmd[2] = opcode
  cmd[3] = data
  logging.debug("prepared serial command: %s", binascii.hexlify(cmd))

  try:
    with serial.Serial(ctx.obj['PORT'], ctx.obj['BAUD']) as sp:
      logging.debug("Opened %s", sp.port)
      sp.write(cmd)
      sp.flush()
  except serial.serialutil.SerialException:
    logging.exception("Serial port communication error:")
    raise SystemExit(1)


def bitmap2int(bitmap_str):
  """Parses an ascii string of bit indicies that shall be set in 1 byte bitmap,
     returning the result as a 1 byte value"""
  bit_idxs = map(int, bitmap_str.split(','))
  output = 0x00
  for bit_idx in bit_idxs:
    output |= (1 << bit_idx)
  return output


@click.group()
@click.option('-p', '--port', type=click.Path(exists=True), required=True)
@click.option('-b', '--baud', default=115200, show_default=True)
@click.option('--verbose', is_flag=True)
@click.option('--debug', is_flag=True)
@click.pass_context
def cli(ctx, port, baud, verbose, debug):
  # Init logging at the correct verbosity
  if debug:
    logging.basicConfig(level=logging.DEBUG)
  elif verbose:
    logging.basicConfig(level=logging.INFO)
  else:
    logging.basicConfig(level=logging.WARNING)
  # ensure that ctx.obj exists and is a dict (in case `cli()` is called
  # by means other than the `if` block below)
  ctx.ensure_object(dict)
  ctx.obj['PORT'] = port
  ctx.obj['BAUD'] = baud


@cli.command(short_help="Enters programming mode on the target board")
@click.option('-t', '--target', default=0, show_default=True, help="Start channel of the board that shall be placed into programming mode")
@click.option('--no-test', is_flag=True, help="Disables the actuation test upon entering programming mode")
@click.pass_context
def enter_prog(ctx, target, no_test):
  do_serial_command(ctx, 0xff, target)
  if not no_test:
    time.sleep(0.1)
    do_serial_command(ctx, 0xf0, 5)


@cli.command(short_help="Exits programming mode on all connected boards currently in programming mode")
@click.option('--save/--no-save', help="Saves the programming to the EEPROM upon exit")
@click.pass_context
def exit_prog(ctx, save):
  if save:
    do_serial_command(ctx, 0xfe, 0xfe)
  else:
    logging.warning("Programming mode exited without saving changes!")
    do_serial_command(ctx, 0xfe, 0x00)


@cli.command(short_help="Tests all motors in the +ve direction for (--time) tenths of a second each in logical/channel order")
@click.option('--time', default=20, show_default=True, help="Time to actuate motors in tenths of a second")
@click.pass_context
def test(ctx, time):
   do_serial_command(ctx, 0xf0, time)


# All programmable parameters (indexes must match firmware configuration in board.h)
parameter_dict = {'CHANNEL_OFFSET': 0x00,
                  'UNIDIR_ACTUATION_POWER': 0x01,
                  'BIDIR_ACTUATION_POWER': 0x02,
                  'REVERSE_MASK': 0x03,
                  'MOTOR_ORDER': 0x04}

# All motor orders (must also be consistent with definition in board.h)
motor_orders = {'012': 0x00,
                '120': 0x01,
                '201': 0x02,
                '021': 0x03,
                '210': 0x04,
                '102': 0x05}

@cli.command(short_help="Sets the specified configuration parameter to the provided value")
@click.option('-o', '--option', type=click.Choice(parameter_dict.keys(), case_sensitive=False), required=True)
@click.option('-t', '--value-type', type=click.Choice(['NUMBER', 'BITMAP', 'INDEX'], case_sensitive=False), default='NUMBER', show_default=True)
@click.option('-v', '--value', type=click.STRING, required=True)
@click.pass_context
def set_option(ctx, option, value_type, value):
  # first preprocess the value per the stated type
  if value_type == 'NUMBER':
    if value.startswith('0x'):
      value = value.removeprefix('0x')
      base = 16
    elif value.startswith('0b'):
      value = value.removeprefix('0b')
      base = 2
    else:
      base = 10
    proc_val = int(value, base)
  elif value_type == 'BITMAP':
    proc_val = bitmap2int(value)
  elif value_type == 'INDEX':
    if option != 'MOTOR_ORDER':
      raise click.BadOptionUsage('-t/--value-type', f"Indexes are only valid for setting the MOTOR_ORDER option")
    elif not (value in motor_orders):
      raise click.BadOptionUsage('-v/--value', f"MOTOR_ORDER must be one of {', '.join(motor_orders)}")
    else:
      proc_val = motor_orders[value]
  else:
    raise RuntimeError("set_parameter received value type not in list of choices! Click bug?")

  # Issue the programming command
  logging.info("Setting option %s to value %s (0x%s)", option, value, hex(proc_val))
  do_serial_command(ctx, parameter_dict[option], proc_val)


if __name__ == '__main__':
  cli(auto_envvar_prefix='RPSCPROG')
