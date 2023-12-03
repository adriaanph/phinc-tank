"""
    A simulation study to assess the confidence level of James Covacich's IQ estiamtes for AGS 4F in 2023.
"""
# If you have missing libraries, run the following on the command line: pip install numpy, matplotlib
import numpy as np
import pylab as plt


def randomselect(all_samples, number):
    """ @return: a randomly selected subset of 'all_samples' """
    indices = np.random.randint(0, len(all_samples), int(number))
    return np.take(all_samples, indices)


def new_population(N_people):
    """ IQ is defined so that the mean over the entire population is 100, with standard deviation of 15 [Wikipedia:Intelligence Quotient]
        @return: a list of IQ scores for N_people """
    return 100 + 15*np.random.randn(int(N_people))


def study_4F(N_years=30, N_tries=100):
    """ A simulation study to assess the reasonableness of James Covacich's results of Dec 2023 """
    
    # Covacich's result
    jc4F_IQs = [118.12, 108.46, 107.30, 106.03, 105.95, 105.45, 104.88,
                104.78, 104.74, 104.71, 104.65, 104.27, 103.98, 103.84,
                103.84, 103.77, 103.69, 102.22, 103.03, 102.98, 102.97,
                102.69, 102.28, 102.33, 102.16, 100.98, 100.78,  99.57,
                 99.60,  99.44,  98.54,  98.54,  97.93,  97.48,  97.47]
    
    # We'll collect all possible "close matches" to JC's result in the following list
    closest_results = []
    
    
    # Functions for making figures and plots
    new_fig = lambda: plt.figure() # Make a new figure
    plot_hist = lambda IQs, label: (plt.hist(IQs, bins=30, range=(60, 140), density=False, alpha=0.8, label=label), plt.xlabel("IQ"), plt.legend())
    
    # We'll run the analysis below for a number of years with completely independent random groups
    for year in range(N_years):
        
        new_fig()
        # There is roughly 100thousand people living in the enrollment zone of AGS, and that their IQs are representative for Auckland
        agszone_IQs = new_population(100e3)
        plot_hist(agszone_IQs, "AGS zone")
        
        # We assume the population is 50% male & female and randomly select the males i.e. no difference in intelligence between males & females
        agscandidate_IQs = randomselect(agszone_IQs, 0.5*len(agszone_IQs))
        plot_hist(agscandidate_IQs, "AGS zone males")
        
        # Since AGS is a competitive school, let's discard individuals with IQ below 85
        agscandidate_IQs = agscandidate_IQs[agscandidate_IQs >= 85]
        plot_hist(agscandidate_IQs, "AGS candidates")
        
        
        # There is about 550 boys per year in AGS [https://www.educationcounts.govt.nz/find-school/school/population/year?district=&region=&school=54]
        # We'll randomly select them from the available candidates
        agsyear_IQs = randomselect(agscandidate_IQs, 550)
        new_fig()
        plot_hist(agsyear_IQs, "AGS year group")
        
        # Let's assume that there are 15 classes per year (~36 boys per class)
        N_perclass = int(len(agsyear_IQs)/15)
        
        # It is generally agreed that IQ, home environment and ambition all influence scores.
        # It could however be reasonable to make assumptions about the upper and lower ends of the distribution.
        # Let's assume that: 1) the top 2 classes are 50% made up of boys with the highest IQs
        N_brights = int(0.5 * 2*N_perclass)
        # 2) the lowest 5 classes are 50% made up of boys with the lowest IQs
        N_dimms = int(0.5 * 5*N_perclass)
        # Let's focus on the rest of the year group
        bp_IQ = np.percentile(agsyear_IQs, [N_dimms/len(agsyear_IQs)*100, 100-N_brights/len(agsyear_IQs)*100])
        agsyearbracket_IQs = agsyear_IQs[ (bp_IQ[0] <= agsyear_IQs) & (agsyear_IQs <= bp_IQ[1]) ]
        plot_hist(agsyearbracket_IQs, "AGS year bracket")
        
        # How likely is it to select a class at random that matches Covacich's result?
        plot_hist(jc4F_IQs, "Covacich's 4F group")
        jc4F_stats = [np.median(jc4F_IQs), (np.max(jc4F_IQs)-np.min(jc4F_IQs))/2] # Median & 1/2 range
        
        closeness = 0.1 # 10%
        matches = [] # True if the selected class matches jc4F
        for i in range(N_tries): # Try many times to select a random class 
            class_IQs = randomselect(agsyearbracket_IQs, N_perclass)
            class_stats = [np.median(class_IQs), (np.max(class_IQs)-np.min(class_IQs))/2] # Median & 1/2 range
            is_match = (abs(class_stats[0]/jc4F_stats[0] - 1) < closeness) and (abs(class_stats[1]/jc4F_stats[1] - 1) < closeness) # True if differ by less than closeness
            matches.append(is_match)
            if is_match:
                plot_hist(class_IQs, "A bracket class close to JC's")
                closest_results.append(class_IQs)
        likelihood = np.count_nonzero(matches)/len(matches) * 100
        print(f"Year {year} likelihood of encountering Covacich's result: {likelihood:.1f} %")
    
    
        # The following prevents making plots for years after the first one
        new_fig = lambda: None
        plot_hist = lambda IQs, label: None

    
    # Compare JC's result against the average distribution of the closest matches
    plt.figure()
    plt.hist(np.concatenate(closest_results), bins=30, range=(90, 130), density=True, alpha=0.8, label="Average of Matches")
    plt.hist(jc4F_IQs, bins=30, range=(90, 130), density=True, alpha=0.8, label="Covacich's 4F group")
    plt.xlabel("IQ"); plt.legend()



if __name__ == "__main__":
    study_4F()
    plt.show()