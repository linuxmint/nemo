#!/usr/bin/python3
import sys
import xlrd
import locale

path = sys.argv[1]

try:
    book = xlrd.open_workbook(path)
except Exception as e:
    print("Can't read '%s': %s" % (path, e))
    exit(1)

date_format = locale.nl_langinfo(locale.D_FMT)

for sheet in book.sheets():
    for row in sheet.get_rows():
        had_content = False
        for cell in row:
            if cell.ctype == xlrd.XL_CELL_TEXT:
                print(cell.value, end=" ")
                had_content = True
            elif cell.ctype == xlrd.XL_CELL_DATE:
                dt = xlrd.xldate_as_datetime(cell.value, 0)
                print(dt.strftime(date_format), end=" ")
                had_content = True

        if had_content:
            # end of row newline (but only if the row wasn't blank)
            print("")
exit(0)