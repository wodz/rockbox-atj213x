bool i2c_wait_finish(unsigned iface)
{
    unsigned tmo = current_tick + HZ/10;

    while (!TIME_AFTER(current_tick, tmo))
    {
        if (I2C_STAT(iface) & 0x80)
        {
            yield();
        }
        else
        {
            I2C_STAT(iface) = 0x80;
            return true;
        }
    }
    return false;
}

bool i2c_start(unsigned iface, unsigned char address, bool repeat)
{
    I2C_ADDR(iface) = address;

    if (repeat)
        I2C_CTL(iface) = 0x8e;
    else
        I2C_CTL(iface) = 0x86;

    return i2c_wait_finish();
}

void i2c_stop(unsigned iface)
{
    unsigned tmo = current_tick + HZ/10;
    I2C_CTL = 0x8b;

    while (!TIME_AFTER(current_tick, tmo))
    {
        if ((I2C_STAT(iface) & STPD) == 0)
            yield();
    }

    I2C_STAT(iface) = 0x40;
}

bool i2c_write_byte(unsigned iface, unsigned char data)
{
    I2C_DAT(iface) = data;
    I2C_CTL(iface) = 0x82;

    return i2c_wait_finish();
}

unsigned char i2c_read_byte(unsigned iface, bool ack)
{
    if (ack)
        I2C_CTL(iface) = 0x82;
    else
        I2C_CTL(iface) = 0x83;

    if (i2c_wait_finish())
        return I2C_DAT(iface);

    return 0;
}

void atj213x_i2c_reset(unsigned iface)
{
    I2C_CTL(iface) = 0;
    I2C_CTL(iface) |= EN;
}

void atj213x_i2c_init(unsigned iface)
{
    CMU_DEVCLKEN |= I2C; /* enable i2c APB clock */
    CMU_GPIO_MFCTL1 &= ~I2C1SS; // check

    atj213x_i2c_reset(iface);
}

void atj213x_i2c_set_speed(unsigned iface, unsigned i2cfreq)
{
    unsigned int pclkfreq = atj213x_get_pclk();

    clkdiv = (pclkfreq + (i2cfreq - 1))/i2cfreq;
    clkdiv = (clkdiv + 15)/16;

    I2C_CLKDIV(iface) = clkdiv;
}

/*
function ATJ.i2c.transmit(slave_addr, buffer, send_stop)
    i2c_start(bit32.band(slave_addr, 0xfe))

    for i=1, #buffer do
        i2c_write_byte(buffer[1])
    end

    if (send_stop ~= 0) then
        i2c_stop()
    end
end

function ATJ.i2c.receive(slave_addr, length)
    local res = {}

    i2c_start(bit32.bor(slave_addr, 1))

    for i=1, length-1 do
        res[i] = i2c_read_byte(true)
    end

    res[length] = i2c_read_byte(false)

    i2c_stop()

    return res
end
*/
