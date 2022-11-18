""" Basics of "Hailtone" number sequence. See <https://en.wikipedia.org/wiki/Collatz_conjecture>

    @requires: numpy
    
    @author: adriaan & arami peens-hough
"""
import numpy as np


def num2digits(xyz):
    """ Splits a number xyz into its digits, returns [x,y,z] """
    digits = [int(d) for d in str(xyz)]
    return digits

def is_happy(number, max_cycles=999):
    for c in range(max_cycles):
        digits = num2digits(number)
        print(number, digits, end=" => ") # Not advancing to a new line
        number = np.sum([x**2 for x in digits])
        print(number)
        if (number == 1):
            print("happy!")
            return True
    print(f"not happy after {c} iterations")
    return False


if __name__ == "__main__": # Running as stand-alone program
    is_happy(146)
