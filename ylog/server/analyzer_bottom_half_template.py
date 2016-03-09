import os
import re
import sys
import optparse
import shutil
import struct

class ytag_fd(object):
    pass

def ytag_extract_file_ytfd(file_name):
    if file_name not in ytfd_file:
        file_name_entry = ytag_fd() # create a new class object
        file_name_entry.ytfds = {}
        file_name_entry.name = os.path.join(ytag_folder_abspath, file_name)
        file_name_entry.count = 0
        ytfd_file[file_name] = file_name_entry
    else:
        ytfd_file[file_name].count = ytfd_file[file_name].count + 1
    count = ytfd_file[file_name].count
    ytfd = ytag_fd() # create a new class object
    ytfd.name = os.path.join(ytag_folder_abspath, '%04d.%s' % (count, file_name))
    ytfd.fd = open(ytfd.name, 'wb')
    ytfd.size = 0
    ytfd_file[file_name].ytfds[count] = ytfd
    return ytfd

def ytag_extract_file(fd_src, ytfd):
    while True:
        ytag = fd_src.read(8)
        if len(ytag) == 8:
            ytags = struct.unpack('<II', ytag)
            tag = ytags[0]
            dlen = ytags[1]
            if dlen < YTAG_STRUCT_SIZE:
                print('Fatal dlen = %d at pos %d' % (dlen, fd_src.tell()))
                return -1
            dlen = dlen - YTAG_STRUCT_SIZE
            if tag == YTAG_TAG_RAWDATA: # file data
                if dlen == 0:
                    continue
                rdata = fd_src.read(dlen)
                ytfd.fd.write(rdata)
                rdlen = len(rdata)
                ytfd.size = ytfd.size + rdlen
                if rdlen != dlen:
                    print('Fatal rdlen = %d, but dlen = %d at pos %d' % (rdlen, dlen, fd_src.tell()))
                    return -1
            elif tag == YTAG_TAG_NEWFILE_BEGIN: # begin of a new file
                if dlen == 0:
                    new_file_name = 'ytag_' + merged
                else:
                    new_file_name = fd_src.read(dlen).decode('utf-8')
                    # python3 use decode() to remove "b''", 'utf-8' let us can decode chinese character
                if ytag_extract_file(fd_src, ytag_extract_file_ytfd(new_file_name)) < 0:
                    return -1
            elif tag == YTAG_TAG_NEWFILE_END or tag == YTAG_MAGIC: # end of a file or ytag file magic
                if dlen > 0:
                    rdata = fd_src.read(dlen)
                if tag == YTAG_TAG_NEWFILE_END:
                    ytfd.fd.close()
                    return 0
            else:
                print('Fatal tag = 0x%08x at pos %d does not support' % (tag, fd_src.tell()))
                return -1
        elif len(ytag) == 0:
            return 0
        else:
            print('Fatal len(ytag) = %d at pos %d' % (len(ytag), fd_src.tell()))
            return -1

def ytag_parse(ytag_logfile):
    global ytfd_file
    ytfd_file = {}
    sys.setrecursionlimit(90000) # otherwise will meet "RuntimeError: maximum recursion depth exceeded in cmp"
    with open(os.path.join(analyzer_abspath, ytag_logfile), 'rb') as fd_src:
        ytag_extract_file(fd_src, ytag_extract_file_ytfd(merged))
    for file_name in ytfd_file:
        ytfds = ytfd_file[file_name].ytfds
        for ytfd_index in ytfds:
            ytfd = ytfds[ytfd_index]
            ytfd.fd.close() # must close, otherwise can't work in windows python
            if ytfd.size == 0:
                os.remove(ytfd.name);
                ytfd_file[file_name].count = ytfd_file[file_name].count - 1
            else:
                ytfd_latest = ytfd
        if ytfd_file[file_name].count == 0:
            os.rename(ytfd_latest.name, ytfd_file[file_name].name)

def merge_logs(files, output):
    with open(output, 'wb') as alllog:
        for f in files:
            with open(os.path.join(analyzer_abspath, f), 'rb') as sublog:
                shutil.copyfileobj(sublog, alllog)

def split_log(infiles, logdict):
    fddict = {}
    keys = logdict.keys()
    for key in keys:
        fddict[key] = open(os.path.join(analyzer_abspath, logdict[key]), 'w')

    for eachfile in infiles:
        with open(os.path.join(analyzer_abspath, eachfile), 'r') as f:
            for line in f.readlines():
                if line[0:id_token_len] in keys:
                    fddict[line[0:id_token_len]].write(line[id_token_len:])

    for key in keys:
        fddict[key].close()

def main():
    global analyzer_abspath
    global ytag_folder_abspath
    analyzer_abspath = os.path.dirname(os.path.abspath(sys.argv[0]))
    parser = optparse.OptionParser()
    parser.add_option('-r', dest='remove', default=False, action='store_true', help='remove the original log files')
    parser.add_option('-m', dest='merge', default=False, action='store_true', help='merge all the log files')
    parser.add_option('-R', dest='reverse', default=True, action='store_false', help='sort the files reverse')
    parser.add_option('-d', dest='debug', default=False, action='store_true', help='output some debug info')
    options, args = parser.parse_args()

    if not args:
        allfiles = os.listdir(os.path.join(analyzer_abspath, logpath))
        pat = re.compile('.*\.?[0-9]+$')
        logfilenames = [f for f in allfiles if pat.match(f)]
    else:
        logfilenames = args;

    logfilenames.sort(reverse = options.reverse)

    if options.debug:
        print(logfilenames)

    if options.merge or (not logdict or YTAG):
        if options.merge: # First priority checking
            merge_logs(logfilenames, os.path.join(analyzer_abspath, merged))
        elif YTAG: # Second priority checking
            ytag_folder_abspath = os.path.join(analyzer_abspath, ytag_folder)
            if os.access(ytag_folder_abspath, os.F_OK):
                shutil.rmtree(ytag_folder_abspath)
            os.mkdir(ytag_folder_abspath)
            tmp_file = os.path.join(ytag_folder_abspath, 'temp.log')
            merge_logs(logfilenames, tmp_file)
            ytag_parse(tmp_file)
            os.remove(tmp_file)
        else: # Last priority checking
            merge_logs(logfilenames, os.path.join(analyzer_abspath, merged))
    else:
        split_log(logfilenames, logdict)

    if options.remove:
        for log in logfilenames:
            os.remove(os.path.join(analyzer_abspath, log))
        os.remove(sys.argv[0])

if __name__ == '__main__':
    main()
