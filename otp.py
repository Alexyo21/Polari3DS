"""
Module to decrypt 3DS retail and dev OTPs
Derived from otptool: https://github.com/SciresM/otptool
"""

import argparse
import hashlib

from Cryptodome.Cipher import AES

OTP_MAGIC = b'\xde\xad\xb0\x0f'

OTP_KEY_RETAIL = bytearray([0x06, 0x45, 0x79, 0x01, 0xd4, 0x85, 0xa3, 0x67,
                            0xac, 0x4f, 0x2a, 0xd0, 0x1c, 0x53, 0xcf, 0x74])
OTP_IV_RETAIL = bytearray([0xba, 0x4f, 0x59, 0x9b, 0x0a, 0xe1, 0x12, 0x2c,
                           0x80, 0xe1, 0x3f, 0x68, 0x65, 0xc4, 0xfa, 0x49])

OTP_KEY_DEV = bytearray([0x9c, 0xea, 0x65, 0x6e, 0x96, 0x28, 0x7b, 0xc1,
                         0x8f, 0xd7, 0xd4, 0xbb, 0xd4, 0x58, 0x72, 0x70])
OTP_IV_DEV = bytearray([0x3e, 0x00, 0x9a, 0xfb, 0xb8, 0x5f, 0x13, 0x62,
                        0x72, 0x68, 0x75, 0x7c, 0xe3, 0xb4, 0xbe, 0xcc])


def decrypt_otp(inp_path, out_path, is_dev=False):
    """
    Decrypts OTP via AES CBC cipher using keys above

    inp_path: Path to encrypted OTP
    out_path: Path to store decrypted OTP
    is_dev: Whether or not the OTP originates from a dev console

    return: None
    """

    if is_dev:
        otp_key = OTP_KEY_DEV
        otp_iv = OTP_IV_DEV
    else:
        otp_key = OTP_KEY_RETAIL
        otp_iv = OTP_IV_RETAIL

    cipher = AES.new(otp_key, AES.MODE_CBC, otp_iv)

    with open(inp_path, 'rb') as inp:
        enc_otp = inp.read()

    dec_otp = cipher.decrypt(enc_otp)
    if dec_otp[:0x04][::-1] != OTP_MAGIC:
        raise ValueError('OTP magic mismatch')

    calc_hash = hashlib.sha256(dec_otp[:0xE0]).digest()
    if calc_hash != dec_otp[0xE0:]:
        raise ValueError('OTP hash mismatch')

    with open(out_path, 'wb') as out:
        out.write(dec_otp)


def main():
    """Main function to be ran when running script as __main__"""

    parser = argparse.ArgumentParser(description='OTP Decrypter')
    parser.add_argument(
        '-i',
        '--input',
        dest='inp',
        help='Path to encrypted OTP',
        required=True
    )

    parser.add_argument(
        '-o',
        '--output',
        dest='out',
        help='Path to store decrypted OTP',
        required=True
    )

    parser.add_argument(
        '-d',
        '--dev',
        dest='is_dev',
        help='Flag if OTP is from a dev unit',
        action='store_true'
    )

    args = parser.parse_args()
    decrypt_otp(args.inp, args.out, is_dev=args.is_dev)


if __name__ == '__main__':
    main()
