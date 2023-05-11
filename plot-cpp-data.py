import numpy as np
import pandas as pd
import json
import matplotlib.pyplot as plt
import sys

hypparam_file = open(sys.argv[2], 'r')
hypparam = json.load(hypparam_file)
simsDFall = pd.read_csv(sys.argv[1], dtype={'source_timestamp': int, 'target_timestamp':int, 'score':float})
simsDF = simsDFall.loc[simsDFall['score']>0.7]

start = int(hypparam["start"])
end = int(hypparam["end"])
skip = int(hypparam["skip"])
minNotes = int(hypparam["minNotes"])
maxNotes = int(hypparam["maxNotes"])
thresh = float(hypparam["thresh"])


def plot_above_thresh():
    # plotting all mathes > threshold score
    plt.rcParams["figure.figsize"] = [7, 5]
    plt.rcParams["figure.autolayout"] = True

    #title = "All matches >"+str(thresh)+", for \nMin Notes=" + str(minNotes) + " notes, Max Notes=" + str(maxNotes)
    title = "All matches > 0.7"
    simsDF.plot.scatter(x="source_timestamp", y="target_timestamp", s='score', title=title)
    plt.savefig("above_thresh_test.png")

def plot_best_matches():
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
    plt.savefig("best_matches_test.png")

    return distribution_hist, close_matches

def plot_expect_diff_histogram(distribution_hist):
    mean = np.mean(distribution_hist)
    std = np.std(distribution_hist)
    print("Distribution Mean:{:.2f}".format(mean))
    print("Distribution Standad Deviation:{:.2f}".format(std))
    plt.hist(distribution_hist,bins=int(max(distribution_hist)-min(distribution_hist)))
    plt.title("Histogram of time difference from expected")
    plt.xlabel("Time difference (ms)")
    plt.ylabel("Frequency")
    plt.savefig("expect_diff_histogram.png")

# def plot_alignment_src_targ(distribution_hist, close_matches):
#     close_matches_array = np.array(close_matches)
#     distribution_hist_array = np.array(distribution_hist)
#     print(close_matches_array.shape)
#     close_matches_df = pd.DataFrame(data = close_matches_array, columns=['source_timestamp', 'target_timestamp','score','source_id_start','source_id_end','target_id_start','target_id_end','match_len','match_time'])
#     close_matches_df['deviation'] = distribution_hist_array

#     close_matches_sorted = close_matches_df.sort_values(by=['deviation'])

#     worst_matches = np.array(close_matches_sorted.iloc[-k:],dtype=float)

#     print(close_matches_df.iloc[0])
#     print(worst_matches.shape)
#     print(worst_matches.astype(int))
#     for match in worst_matches:
#         print(match.astype(int))
#         source_start = int(match[0])
#         target_start = int(match[1])
#         score = match[2]
#         source_id_start = int(match[3])
#         source_id_end = int(match[4])
#         target_id_start = int(match[5])
#         target_id_end = int(match[6])
#         match_len = int(match[7])
#         match_time = int(match[8])
        
#         # plot worst match (in terms of distance from expected timestamp)
#         diff = source_start - target_start
#         sequence1 = np.copy(notes[source_id_end:source_id_start+3])
#         sequence1[:,0] = sequence1[:,0]-diff
#         sequence2 = np.copy(notes[target_id_end:target_id_start+3])
#         sequence2[:,0] = sequence2[:,0]
#         sequence3 = sequence2.copy()
#         sequence3[:,0] = sequence2[:,0]-diff+191400
        
#         display_expected_actual(sequence1,sequence2,source_start,target_start,source_start-diff,target_start,source_start-191400,diff,score,match_time,l1="Source",l2="Matching Target",l3="Expected Target")
#         display_expected_actual(sequence1,sequence3,source_start,target_start,source_start-191400,source_start-191400,target_start,diff,score,match_time,l1="Source",l2="Expected Target",l3="Matching Target")
        
#         a = notes[source_id_start-2:source_id_start+3]
#         b = notes[target_id_start-2:target_id_start+3]
#         print("Source end notes:\n",a)
#         print("Source end notes - 191400:",a[:,0] - 191400)
#         print("Target end notes:\n",b)
#         print("Source diff between notes:",a[1:,0] - a[:-1,0])
#         print("Target diff between notes:",b[1:,0] - b[:-1,0])
#         print("Source time - last note:",match[0]-notes[int(match[3])-1][0])
#         print("Target time - last note:",match[1]-notes[int(match[5])-1][0])
#         print()

plot_above_thresh()
distribution_hist, close_matches = plot_best_matches()
plot_expect_diff_histogram(distribution_hist)