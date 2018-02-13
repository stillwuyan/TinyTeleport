#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys
import pycurl
from multiprocessing import Queue, Process
from queue import Empty
import time
import os
from urllib.parse import urljoin, urlparse
from datetime import datetime

FUNC_MAP = {
    '.m3u8': '1.m3u8',
    '.pdf': '1.pdf',
    '.mp4': '1.mp4',
}
CHUNK_SIZE = 1 * 1024 * 1024    # Chunk size: 1M
CHUNK_MARGIN = 2 * CHUNK_SIZE
QUEUE_WAIT_SECOND = 5
QUEUE_WAIT_TIMES = 6            # Total 30s
INFO_FILE = 'record.log'
LOG_FILE = 'download.log'

def progress(download_total, downloaded, uploaded_total, upload):
    debug("To be downloaded: {}".format(download_total))
    debug("Downloaded : {}".format(downloaded))

def parse_m3u8(queue, url, file):
    m3u8_url = None
    ts_list = []
    with open(file, 'r') as f:
        for line in f:
            result = urlparse(line.strip())
            if result.path.endswith('.m3u8'):
                m3u8_url = result.path
                break
            elif result.path.endswith('.ts'):
                ts_list.append(result.path)
    if m3u8_url is not None:
        new_url = urljoin(url, m3u8_url)
        queue.put(new_url)
        info({'from': url, 'to': url})
    elif len(ts_list) > 0:
        for ts in ts_list:
            new_url = urljoin(url, ts)
            queue.put(new_url)

def get_size(url):
    c = pycurl.Curl()
    c.setopt(c.URL, url)
    c.setopt(c.NOBODY, True)
    code = -1
    try:
        c.perform()
        code = c.getinfo(c.RESPONSE_CODE)
        size = c.getinfo(c.CONTENT_LENGTH_DOWNLOAD)
        if code != 200:
            error('Get file size failed: {}'.format(url))
            error('Reason: response code is {}'.format(code))
    except Exception as e:
        error('Get file size failed: {}'.format(url))
        error('Reason: {}'.format(e))
        size = 0
    return size, code!=200

def get_file(url, output_file):
    debug('Download beginning: {}'.format(url))
    c = pycurl.Curl()
    c.setopt(c.URL, url)
    # c.setopt(c.NOPROGRESS, True)
    # c.setopt(c.PROGRESSFUNCTION, progress)
    retry = False
    with open(output_file, 'wb') as f:
        c.setopt(c.WRITEDATA, f)
        try:
            c.perform()
            code = c.getinfo(c.RESPONSE_CODE)
            if code is 200:
                debug('Download successfully: {}'.format(url))
                info({'url':url, 'file':output_file}) if not output_file.endswith('.ts') else None
            else:
                error('Download failed: {}'.format(url))
                error('Reason: response code is {}'.format(code))
                retry = True
        except Exception as e:
            error('Download failed: {}'.format(url))
            error('Reason: {}'.format(e))
            retry = True
        return retry

def get_large_file(url, output_file, size):
    debug('Download beginning: {}'.format(url))
    c= pycurl.Curl()
    c.setopt(c.URL, url)
    retry = False
    with open(output_file, 'wb') as f:
        c.setopt(c.WRITEDATA, f)
        count = size // CHUNK_SIZE
        index = 0
        while index < count:
            try:
                lim_l = index * CHUNK_SIZE
                lim_u = (index+1)*CHUNK_SIZE if (index+1) < count else size
                c.setopt(c.RANGE, '{}-{}'.format(lim_l, lim_u-1))
                c.perform()
                code = c.getinfo(c.RESPONSE_CODE)
                if code == 206:
                    index += 1
                    debug('Downloading...: {}[{}:{}]'.format(url, lim_u, size))
                else:
                    error('Download failed: {}[{}-{}]'.format(url, lim_l, lim_u-1))
                    f.seek(lim_l)
            except Exception as e:
                error('Download failed: {}'.format(url))
                error('Reason: {}'.format(e))
    debug('Download successfully: {}[{}:{}]'.format(url, os.path.getsize(output_file), size))
    info({'url':url, 'file':output_file}) if not output_file.endswith('.ts') else None
    return retry

def download(queue, output):
    times = 0
    while times < QUEUE_WAIT_TIMES:
        try:
            url = queue.get(True, 5)
        except Empty:
            times += 1
            continue

        requeue = False
        result = urlparse(url)
        output_file = os.path.join(output, result.path[1:])
        if not os.path.exists(os.path.dirname(output_file)):
            os.makedirs(os.path.dirname(output_file), exist_ok=True)
        size, error = get_size(url)

        if error:
            error('Drop url: {}'.format(url))
            continue
        elif os.path.exists(output_file) and os.path.getsize(output_file) == size:
            debug('Already downloaded: {}'.format(url))
        elif size < CHUNK_MARGIN:
            requeue = get_file(url, output_file)
        else:
            requeue = get_large_file(url, output_file, size)

        if requeue:
            queue.put(url)
        elif output_file.endswith('.m3u8'):
            parse_m3u8(queue, url, output_file)

def dispatch(url, queue):
    if len(url) == 0:
        return
    queue.put(url)

def debug(msg):
    #print('[{}]{}'.format(os.getpid(), msg))
    s = '[{}][{}]{}'.format(datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f'), os.getpid(), msg)
    with open(LOG_FILE, 'at') as f:
        f.write(s + '\n')

def error(msg):
    print('[{}]{}'.format(os.getpid(), msg), sys.stderr)
    s = '[{}][{}]{}'.format(datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f'), os.getpid(), msg)
    with open(LOG_FILE, 'at') as f:
        f.write(s + '\n')

def info(obj):
    with open(INFO_FILE, 'at') as f:
        f.write(str(obj) + '\n')

def main():
    if len(sys.argv) != 3:
        print("Error arguments!\n\nUsage: python main.py <input file> <output path>\n")
        return
    output = sys.argv[2]
    if not os.path.exists(output):
        os.makedirs(output)
    queue = Queue()
    f= lambda _: Process(target=download, args=(queue, output))
    process_pool = map(f, range(10))
    for process in process_pool:
        process.start()
    with open(sys.argv[1]) as f:
        for line in f:
            dispatch(line.strip(), queue)
    for process in process_pool:
        process.join()

if __name__ == '__main__':
    main()
