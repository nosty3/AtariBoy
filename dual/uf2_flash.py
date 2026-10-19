"""RP2040-E14 workaround. See datasheet 2.8.4.2 and Appendix B.
The ROM groups erase bookkeeping by UF2 block_no//16, not flash address.
Always emit all sixteen pages of each touched sector, sorted by address.
"""
def complete_sectors(blocks, start, end):
    assert start % 4096 == end % 4096 == 0
    result = dict(blocks)
    for address, data in blocks.items():
        assert address % 256 == 0 and len(data) == 256
        sector = address & ~4095
        assert start <= sector and sector + 4096 <= end
        for page in range(sector, sector + 4096, 256):
            result.setdefault(page, b'\xff' * 256)
    return result
