import struct
import pathlib
import matplotlib.pyplot as plt
# import matplotlib.patches as mpatches
# from matplotlib.legend_handler import HandlerPatch
import matplotlib.lines as mlines
import sys
import numpy as np
import datetime


REMOTE_PI_ARM32 = (sys.argv[1] == 'remote') if len(sys.argv) > 1 else False
platform = "Rpi zero W" if REMOTE_PI_ARM32 else "i5 5200u"
suffix = "ARM32" if REMOTE_PI_ARM32 else ""
fname = "/log_ARM32" if REMOTE_PI_ARM32 else ""
print(f"REMOTE_PI_ARM32: {REMOTE_PI_ARM32}\n")

filename_bt = str(pathlib.Path(__file__).parent.resolve()) + '/..' + fname + '/log/bt_searches.bin'
filename_cc = str(pathlib.Path(__file__).parent.resolve()) + '/..' + fname + '/log/close_contacts.bin'
f_bt = open(filename_bt, 'rb')
f_cc = open(filename_cc, 'rb')

#print(f.read(2))

# Decode one entry...

TIMESCALE = 100

searches = []
while True:
    # entry = f.read(6+8+4+8+4)
    if REMOTE_PI_ARM32:
        entry = f_bt.read(6+4+4)
    else:
        entry = f_bt.read(6+8+4)
    if not entry:
        break

    mac_raw = entry[0:6] + b'\x00\x00' ## f.read(6) + b'\x00\x00'
    
    MAC = f"{struct.unpack('<Q', mac_raw)[0]:0>12X}"

    # print(mac_raw, MAC)
    if REMOTE_PI_ARM32:
        start_sec = struct.unpack('<I', entry[6:10])
        start_usec = struct.unpack('<I', entry[10:14])
    else:
        start_sec = struct.unpack('<Q', entry[6:14])
        start_usec = struct.unpack('<I', entry[14:18])
    #end_sec = struct.unpack('<Q', entry[18:26])
    #end_usec = struct.unpack('<I', entry[26:30])
    
    # searches.append([MAC, start_sec[0], start_usec[0], end_sec[0], end_usec[0]])
    searches.append([MAC, start_sec[0], start_usec[0]])

f_bt.close()

close_contacts = []
while True:
    if REMOTE_PI_ARM32:
        entry = f_cc.read(6+4+4)
    else:
        entry = f_cc.read(6+8+4)
    if not entry:
        break

    mac_raw = entry[0:6] + b'\x00\x00' ## f.read(6) + b'\x00\x00'

    MAC = f"{struct.unpack('<Q', mac_raw)[0]:0>12X}"
    if REMOTE_PI_ARM32:
        start_sec = struct.unpack('<I', entry[6:10])
        start_usec = struct.unpack('<I', entry[10:14])
    else:
        start_sec = struct.unpack('<Q', entry[6:14])
        start_usec = struct.unpack('<I', entry[14:18])

    close_contacts.append([MAC, start_sec[0], start_usec[0]])
f_cc.close()

runtime = datetime.timedelta(seconds=len(searches)*10)
print(f"Total mac searches perfomed: {len(searches)}, run time: {runtime} (actual: {runtime/TIMESCALE}) (hh:mm:ss)")



init_time = searches[0][1]*1000*1000 + searches[0][2]
start_times = [entry[1]*1000*1000 + entry[2] - init_time for entry in searches]
indexes = [t for t in range(len(start_times))]

start_times = [entry[0][1]*1000*1000 + entry[0][2] - init_time - entry[1]*100000 for entry in list(zip(searches, indexes))]
del start_times[0]
del indexes[0]

st_np = np.array(start_times)
print(f"{len(st_np) = }")

# index = st_np.index(list(filter(lambda i: i > 1000, st_np))[0])

index = np.argmax(st_np > 100000)

print(index, st_np[index])

start_idx = index - 3
end_idx = index + 5
np.set_printoptions(edgeitems=100)
np.core.arrayprint._line_width = 500




schs_time_only = [(entry[1], entry[2]) for entry in searches]

start_times = [t - 2**32 if t > 1000000 else t for t in start_times]
start_times = [250 if t > 5000 else t for t in start_times]
st_np[st_np > 1000000] -= 2**32 # 32 bits fix

st_np[np.argmax(st_np)] = st_np.mean()
st_np = np.array(start_times)

print(f"Mim: {st_np.min()} us. Mean: {st_np.mean():.1f} us. Max: {st_np.max()} us. Standard deviation is: {st_np.std():.1f} us.")
print(f"Cumulative for bt searches: { sum(st_np) } us. formated: { datetime.timedelta(seconds=sum(st_np) / 1000000) }. (hh:mm:ss)" )
print()

color = 'red'
plt.plot(indexes, start_times, color = color, marker = '*', markersize = 1, linewidth=0.3)
plt.xlabel('Search (10second interval)')
plt.ylabel('Drift from ideal times (us)')
plt.title(f"Bluetooth scan time error (10 seconds) - {platform}")
# plt.show()
imagename = "../report/" + "bt_macs" + suffix + ".png"

min = mlines.Line2D([], [], color='black', marker=r"$min$",
                        markersize=25, markeredgewidth=0.2, linestyle="None", label=f"{st_np.min():.1f} us")
max = mlines.Line2D([], [], color='black', marker=r'$max$',
                          markersize=25, markeredgewidth=0.2, linestyle="None", label=f"{st_np.max():.1f} us")
mu = mlines.Line2D([], [], color='black', marker=r'$\mu$',
                          markersize=8, markeredgewidth=0.2, linestyle="None", label=f"{st_np.mean():.1f} us")
sd = mlines.Line2D([], [], color='black', marker=r'$\sigma$',
                          markersize=8, markeredgewidth=0.2, linestyle="None", label=f"{st_np.std():.1f} us")
plt.legend(handles=[min, mu, max, sd])

plt.grid(color='grey', linestyle='dotted', linewidth=0.5, axis='y', alpha=0.3)
fig = plt.gcf()
fig.set_size_inches(tuple(fig.get_size_inches()*1.6))
plt.savefig(imagename, dpi = 120, bbox_inches = 'tight')




MAX_HIST = 20*6
MacSearches = ['0']*MAX_HIST
cc_q = []
RUNTIME_MINUTES = 30*24*60
TIMESCALE = 100
# CLOSE_CONTACT_WAIT = 36*1000000
CLOSE_CONTACT_WAIT = 14*24*60*60*1000000

timediffs = []

for idx, search in enumerate(searches):
    # print(idx, search)
    MacSearches[idx % MAX_HIST] = search
    # print(f"\ncnt counter for {idx} MAC: {MacSearches[idx % MAX_HIST]}")
    for cnt in range(1, MAX_HIST):
        # print((idx + cnt) % MAX_HIST, end=' ')
        if MacSearches[idx % MAX_HIST][0] == MacSearches[(idx + cnt) % MAX_HIST][0]:
            # print(search[0])
            # for i, item in enumerate(MacSearches):
            #     if item == '0':
            #         break
            #     print("MacSearches", i, item)
            # print()
            timediff = (MacSearches[idx % MAX_HIST][1]-MacSearches[(idx + cnt) % MAX_HIST][1])*1000000+(MacSearches[idx % MAX_HIST][2]-MacSearches[(idx + cnt) % MAX_HIST][2])
            if timediff > 4*60*1000*1000 / TIMESCALE:
                # print("Time higher than 4 minutes...")
                # print(f"Need to add to close contacts: MAC: {MacSearches[idx % MAX_HIST]}, with timediff: {timediff}")
                cc_q.append(MacSearches[idx % MAX_HIST])
                # for i, item in enumerate(cc_q):
                #     print("cc_q:", i, item, item[1]*1000000+item[2])
                ###################
                # try:
                contact = close_contacts.pop(0)
                # print(cc_q[-1], contact, MacSearches[idx % MAX_HIST])
                # try:
                assert cc_q[-1][0] == contact[0]
                # except:
                    # print(len(close_contacts))
                    # sys.exit()
                # print(MacSearches[idx % MAX_HIST], contact)
                if MacSearches[idx % MAX_HIST][0] == contact[0]:
                    # timediffs.append((contact[1]-MacSearches[idx % MAX_HIST][1])*1000000+(contact[2]-MacSearches[idx % MAX_HIST][2]))
                    timediffs.append(((contact[1]*1000000+contact[2])-(MacSearches[idx % MAX_HIST][1]*1000000+MacSearches[idx % MAX_HIST][2])) - CLOSE_CONTACT_WAIT )
                else:
                    print("Something is off!")
                break


cc_td = np.array(timediffs)
print(f"Total contacts added to close contacts: {len(timediffs)}")


indexes = [t for t in range(len(timediffs))]
# plt.plot(indexes, timediffs, color='green' ) #plot the data

fig, ax = plt.subplots()
ax.plot(indexes, timediffs, color='green', marker = '*', markersize = 1, linewidth=0.3)
ax.ticklabel_format(useOffset=False, style='plain')
# plt.xticks(range(0,len(timediffs), 2), range(1,len(timediffs)+1, 1)) #set the tick frequency on x-axis

plt.ylabel('timediffs (us)') #set the label for y axis
plt.xlabel('index') #set the label for x-axis
plt.title(f"Close Contact error from target (14 days) - {platform}")
imagename = "../report/" + "cc_macs" + suffix + ".png" 
min = mlines.Line2D([], [], color='black', marker=r"$min$",
                        markersize=25, markeredgewidth=0.2, linestyle="None", label=f"{cc_td.min():.1f} us")
max = mlines.Line2D([], [], color='black', marker=r'$max$',
                          markersize=25, markeredgewidth=0.2, linestyle="None", label=f"{cc_td.max():.1f} us")
mu = mlines.Line2D([], [], color='black', marker=r'$\mu$',
                          markersize=8, markeredgewidth=0.2, linestyle="None", label=f"{cc_td.mean():.1f} us")
sd = mlines.Line2D([], [], color='black', marker=r'$\sigma$',
                          markersize=8, markeredgewidth=0.2, linestyle="None", label=f"{cc_td.std():.1f} us")
plt.legend(handles=[min, mu, max, sd])
plt.grid(color='grey', linestyle='dotted', linewidth=0.5, axis='y', alpha=0.3)
fig = plt.gcf()
fig.set_size_inches(tuple(fig.get_size_inches()*1.6))

if False:
    plt.savefig(imagename, dpi = 120, bbox_inches = 'tight')


filename_uc = str(pathlib.Path(__file__).parent.resolve()) + '/..' + fname + '/log/uploaded_contacts.bin'
filename_uc_cnt = str(pathlib.Path(__file__).parent.resolve()) + '/..' + fname + '/log/uploaded_contacts_counter.bin'
f_uc = open(filename_uc, 'rb')
f_uc_cnt = open(filename_uc_cnt, 'rb')

uc_cnt = []
while True:
    # entry = f.read(6+8+4+8+4)
    if REMOTE_PI_ARM32:
        entry = f_uc_cnt.read(2+4+4)
    else:
        entry = f_uc_cnt.read(2+6+6)
    if not entry:
        break
    total_contacts = struct.unpack('<H', entry[0:2])[0]
    if REMOTE_PI_ARM32:
        tv_sec = struct.unpack('<I', entry[2:6])[0]
        tv_usec = (struct.unpack('<I', entry[6:10]))[0]
    else:
        tv_sec = (struct.unpack('<Q', entry[2:2+8]))[0]
        tv_usec = (struct.unpack('<I', entry[2+8:2+8+6]))[0]

    uc_cnt.append([total_contacts, tv_sec, tv_usec])
f_uc_cnt.close()

uc =[]
cumsum = 0
""" while True:
    entry = f_uc.read(6)
    if not entry:
        break
 """
for upload in uc_cnt:
    cumsum = upload[0]
    # print(entry[0:6])
    MACS = ["{:0>12X}".format(struct.unpack('<Q', f_uc.read(6) + b'\x00\x00')[0]) for cnt in range(upload[0])]
    uc.append(MACS)

f_uc.close()

init_time
for it, upload in enumerate(uc):
    print(f"Uploaded: {len(upload)} contacts, {((uc_cnt[it][1]*10**6 + uc_cnt[it][2] - init_time) / 10**6):.1f} s from the start (accelerated)")