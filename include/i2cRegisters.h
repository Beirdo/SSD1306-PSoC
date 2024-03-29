/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

#ifndef __i2cRegister_h__
#define __i2cRegister_h__
    
#include "project.h"
    
uint8 i2c_register_test_device(uint8 addr);
uint8 i2c_register_read(uint8 addr, uint8 regnum);
void i2c_register_write(uint8 addr, uint8 regnum, uint8 value);
uint8 i2c_register_read_noreg(uint8 addr);
void i2c_register_write_noreg(uint8 addr, uint8 value);
uint16 i2c_register_read_msb16(uint8 addr, uint8 basereg);
uint16 i2c_register_read_lsb16(uint8 addr, uint8 basereg);
uint16 i2c_register_read16be(uint8 addr, uint8 regnum);
void i2c_register_write16be(uint8 addr, uint8 regnum, uint16 value);
void i2c_register_write_buffer(uint8 addr, uint8 regnum, uint8 *buffer, uint8 len);
    
#endif // __i2cRegister_h__

/* [] END OF FILE */
