import numpy as np
import pandas as pd
import json
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import sys
import os

if len(sys.argv) < 2:
    print("Usage: python plot-cpp-data.py <path-to-csv-file> <path-to-hypparam-file> <optional: path-to-output-dir>")
    exit(1)

def midi_to_timeseries(midiPath):
    time_notes = []
    uniqueTypes = [128,144,176] 
            
    with open(midiPath) as midi_events:
        for line in midi_events:
            processedLine = line.split(" ")
    
            processedLine[0] = int(processedLine[0]) # processedLine[0] is timestamp
            processedLine[1] = int(processedLine[1], 16) # processedLine[1] is event type
            processedLine[2] = int(processedLine[2], 16) # processedLine[2] is note
            processedLine[3] = int(processedLine[3],16) # processedLine[3] is velocity

            if processedLine[1] not in uniqueTypes:
                print("unexpected event type got! ", processedLine[1])

            if processedLine[1] == 144:
                time_notes.append([processedLine[0], processedLine[2], processedLine[3]])
                
    return np.array(time_notes)

hypparam_file = open(sys.argv[2], 'r')
hypparam = json.load(hypparam_file)
simsDFall = pd.read_csv(sys.argv[1], dtype={'source_timestamp': int, 'target_timestamp':int, 'score':float, 
'source_id_start':int, 'source_id_end':int, 'target_id_start':int, 'target_id_end':int,
'match_len':int, 'match_time':int})
simsDF = simsDFall.loc[simsDFall['score']>0.7]
notes = midi_to_timeseries(hypparam["midiPath"])
if len(sys.argv) > 3:
    dir = sys.argv[3] + "/"
    if os.path.isdir(dir) == False:
        print("Making directory", dir)
        os.mkdir(dir)
else:
    dir = ""

start = int(hypparam["start"])
end = int(hypparam["end"])
skip = int(hypparam["skip"])
curr_times = np.arange(start, end, skip)
minNotes = int(hypparam["minNotes"])
maxNotes = int(hypparam["maxNotes"])
thresh = float(hypparam["thresh"])


def plot_above_thresh():
    figure = plt.figure(figsize=(7,5))
    figure.autolayout = True
    # plotting all matches > threshold score

    #title = "All matches >"+str(thresh)+", for \nMin Notes=" + str(minNotes) + " notes, Max Notes=" + str(maxNotes)
    title = "All matches > 0.7"
    simsDF.plot.scatter(x="source_timestamp", y="target_timestamp", s='score', title=title)
    plt.savefig(dir+"above_thresh_test.png")

def plot_best_matches():
    figure = plt.figure(figsize=(7,5))
    figure.autolayout = True
    percentage_matches_any = 0
    percentage_matches_thresh = 0
    percentage_matches_line = 0
    percentage_matches_thresh_line = 0
    acc = 50                            #ms accuracy for distance from line

    plt.rcParams["figure.figsize"] = [7, 5]
    plt.rcParams["figure.autolayout"] = True
    bestMatches = []

    toPlot = simsDFall
    # toPlot = simsDFall_old

    total = 0
    distribution_hist = []
    distribution_all = []
    close_matches = []
    min_good_score = 1
    for x in range(start, end, skip):
        y = toPlot.loc[toPlot['source_timestamp'] == x]['score']
        total += 1
        if y.any(): 
            y = y.idxmax()
            percentage_matches_any += 1
            if toPlot['score'].loc[y] > thresh:
                percentage_matches_thresh += 1
                if np.abs(toPlot['target_timestamp'].loc[y]-(x-191400))<acc:
                    percentage_matches_thresh_line += 1
            if np.abs(toPlot['target_timestamp'].loc[y]-(x-191400))<acc:
                if toPlot['score'].loc[y] < min_good_score:
                    min_good_score = toPlot['score'].loc[y]
                distribution_hist.append((toPlot['target_timestamp'].loc[y]-(x-191400)))
                distribution_all.append([x,(toPlot['target_timestamp'].loc[y]-(x-191400))])
                percentage_matches_line += 1
                close_matches.append(toPlot.loc[y].tolist())
        else:
            continue
    #     bestMatch = [toPlot['source_timestamp'].loc[y],toPlot['target_timestamp'].loc[y],toPlot['score'].loc[y],toPlot['source_id_start'].loc[y],toPlot['source_id_end'].loc[y],toPlot['target_id_start'].loc[y],toPlot['target_id_end'].loc[y]]
    #     bestMatches.append(bestMatch)
        plt.scatter(toPlot['source_timestamp'].loc[y],toPlot['target_timestamp'].loc[y],toPlot['score'].loc[y],c='blue')
    # bestMatches = np.array(bestMatches)
    # bestMatchesDF = pd.DataFrame(data=bestMatches, columns=["source_timestamp","target_timestamp", "score", "source_id_start", "source_id_end", "target_id_start", "target_id_end"])   

    plt.plot(np.arange(170000,400000),np.arange(170000,400000))
    plt.plot(np.arange(191400,391400),np.arange(200000))
    percentage_matches_any /= total
    percentage_matches_thresh /= total
    percentage_matches_line /= total
    percentage_matches_thresh_line /= total
    print("Percentage matches found: {:.1f}%".format(percentage_matches_any*100))
    print("Percentage matches >{:.1f} found: {:.1f}%".format(thresh,percentage_matches_thresh*100))
    print("Percentage matches within {:d}ms of line: {:.1f}%".format(acc,percentage_matches_line*100))
    print("Percentage matches >{:.1f} and within {:d}ms of the line: {:.1f}%".format(thresh,acc,percentage_matches_thresh_line*100))
    print("Minimum score of a \"right match\": {:.2f}".format(min_good_score))
    print()
    # print("Number of loops sped up: {:d}".format(num_speedups))
    # print("Percentage loops sped up: {:.2f} - rough estimate".format((num_speedups/(len(curr_times)))*100))

    plt.title("Best match for \nMin Snippet Length=" + str(minNotes) + " notes, Max Notes=" + str(maxNotes))
    plt.xlabel("Source Timestamp (ms)")
    plt.ylabel("Best Match Timestamp")
    plt.savefig(dir+"best_matches_test.png")

    return distribution_hist, distribution_all, close_matches

def plot_expect_diff_histogram(distribution_hist):
    figure = plt.figure(figsize=(7,5))
    figure.autolayout = True
    mean = np.mean(distribution_hist)
    std = np.std(distribution_hist)
    print("Distribution Mean:{:.2f}".format(mean))
    print("Distribution Standad Deviation:{:.2f}".format(std))
    plt.hist(distribution_hist,bins=int(max(distribution_hist)-min(distribution_hist)))
    plt.title("Histogram of time difference from expected")
    plt.xlabel("Time difference (ms)")
    plt.ylabel("Frequency")
    plt.savefig(dir+"expect_diff_histogram.png")

# Plotting k worst matches - matches on the line but farthest from it
def k_worst_matches(distribution_hist, close_matches, k = 1):
    
    close_matches_array = np.array(close_matches)
    distribution_hist_array = np.array(distribution_hist)
    close_matches_df = pd.DataFrame(data = close_matches_array, columns=['source_timestamp', 'target_timestamp','score','source_id_start','source_id_end','target_id_start','target_id_end','match_len','match_time'])
    close_matches_df['deviation'] = distribution_hist_array
    close_matches_sorted = close_matches_df.sort_values(by=['deviation'])
    worst_matches = np.array(close_matches_sorted.iloc[-k:],dtype=float)

    print(worst_matches.astype(int))
    for match in worst_matches:
        source_start = int(match[0])
        target_start = int(match[1])
        score = match[2]
        source_id_start = int(match[3])
        source_id_end = int(match[4])
        target_id_start = int(match[5])
        target_id_end = int(match[6])
        match_len = int(match[7])
        match_time = int(match[8])
        
        # plot worst match (in terms of distance from expected timestamp)
        diff = source_start - target_start
        sequence1 = np.copy(notes[source_id_end:source_id_start+3])
        sequence1[:,0] = sequence1[:,0]-diff
        sequence2 = np.copy(notes[target_id_end:target_id_start+3])
        sequence2[:,0] = sequence2[:,0]
        sequence3 = sequence2.copy()
        sequence3[:,0] = sequence2[:,0]-diff+191400
        
        display_expected_actual(sequence1,sequence2,source_start,target_start,source_start-diff,target_start,source_start-191400,diff,score,match_time,l1="Source",l2="Matching Target",l3="Expected Target")
        plt.savefig(dir+str(k)+"th_worst_match_1.png")
        display_expected_actual(sequence1,sequence3,source_start,target_start,source_start-191400,source_start-191400,target_start,diff,score,match_time,l1="Source",l2="Expected Target",l3="Matching Target")
        plt.savefig(dir+str(k)+"th_worst_match_2.png")
        k -= 1

def display_expected_actual(sequence_source,sequence_target,source_start,target_start,t1,t2,t3,diff,score,lent,l1="Source",l2="Matching Target",l3="Expected Target"):
    fig, ax1 = plt.subplots()

    sourceX = [] # x axis - source timestamp in ms
    sourceY = [] # y axis - note integer repr for source
    targetX = []
    targetY = []
    for i in range(len(sequence_source)):
        sourceX.append(sequence_source[i][0])
        sourceY.append(sequence_source[i][1])

    for i in range(len(sequence_target)):
        targetX.append(sequence_target[i][0])
        targetY.append(sequence_target[i][1])
        
    def s2t(x):
        return x+diff
    def t2s(x):
        return x-diff
    
    ax1.plot(np.full(2, t1),[0,88],linewidth=3, label=l1)
    ax1.plot(np.full(2, t2),[0,88], label=l2)
    ax1.plot(np.full(2, t3),[0,88], label=l3)
    ax1.scatter(sourceX, sourceY, marker='*')
    ax1.scatter(targetX, targetY, marker='.')

    title = "Plot with alignment bw {:s} and {:s}: Source @ {:d} ms, Target @ {:d} ms, Score: {:.2f}, Length (ms): {:d}\
    ".format(l1,l2,source_start,target_start,score,lent)
    
    secax = ax1.secondary_xaxis('top', functions=(s2t, t2s))
    fig.tight_layout()
    plt.title(title)
    ax1.set_xlabel("Source Snippet Time (in ms)")
    secax.set_xlabel("Target Snippet Time (in ms)")
    ax1.set_ylabel("Note (Integer Representation)")
    ax1.legend()

# Plotting distance from line vs source time
def dist_from_line(distribution_all):
    figure = plt.figure(figsize=(10, 3.50))
    distribution_all_arr = np.array(distribution_all)
    plt.plot(distribution_all_arr[:,0],distribution_all_arr[:,1])
    plt.xlabel("Source Timestamp (ms)")
    plt.ylabel("Target Timestamp Deviation from Expected (ms)")
    plt.title("Plot showing time difference betweeen Best Match and Expected Match for every source time in second playthrough")
    plt.savefig(dir+"dist_from_line.png")

def best_match_score():
    # Plotting best match score 
    figure = plt.figure(figsize=(10, 3.50))
    figure.autolayout = True

    # used for legend
    pop_a = mpatches.Patch(color='blue', label='Unexpected match')
    pop_b = mpatches.Patch(color='orange', label='On the line match')

    count = 0
    for x in curr_times:
        y = simsDFall.loc[simsDFall['source_timestamp'] == x]['score'].max()
        s = simsDFall.loc[(simsDFall['source_timestamp'] == x) & (simsDFall['score'] == y)]['match_len'].max()
        if np.isnan(y):
            count += 1
            y = 0
        else:
            if (np.abs(simsDFall.loc[(simsDFall['source_timestamp'] == x) & (simsDFall['score'] == y) & (simsDFall['match_len'] == s)]['target_timestamp']-(x-191400))<100).any():
                color = 'orange'
            else:
                color = 'blue'
            plt.scatter(x,y,1,c=color) # need to include s (len of best match)

    plt.title("Best match score for \nMin Snippet Length=" + str(minNotes) + " notes, Max Notes=" + str(maxNotes))
    plt.xlabel("Source Timestamp (ms)")
    plt.ylabel("Best Match Score")
    plt.legend(handles=[pop_a,pop_b])
    plt.savefig(dir+"best_match_score.png") 

plot_above_thresh()
distribution_hist, distribution_all, close_matches = plot_best_matches()
plot_expect_diff_histogram(distribution_hist)
k_worst_matches(distribution_hist, close_matches, k=5)
dist_from_line(distribution_all)
best_match_score()