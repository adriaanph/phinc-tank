import cv2
import os
from PIL import Image
import numpy as np


FACE_CLASSIFIER = cv2.CascadeClassifier('haarcascade_frontalface_alt.xml')


def show_video(duration_ms, modify_fn=None, close=False):
    """ Show a screen with video, possibly modified. Press ESCAPE key to stop early.
        
        @param modify_fn: a function like f(image)->image, to modify the image before
                          displaying it (default is None).
    """
    bob = cv2.VideoCapture(0)
    bob.set(cv2.CAP_PROP_FRAME_WIDTH,640)
    bob.set(cv2.CAP_PROP_FRAME_HEIGHT,480)
    
    for i in range(0,duration_ms,1):
        ret, frm = bob.read()
        if modify_fn is not None:
            frm = modify_fn(frm)
        cv2.imshow("video",frm)
        if (cv2.waitKey(1) == 27): # ESCAPE key pressed
            break
    bob.release()
    if close:
        cv2.destroyWindow("video")
        cv2.waitKey(1)    


def mod_bw(image):
    return cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
 

def mod_extractface(image, name, rootfolder="faces", debug=True):
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    faces = FACE_CLASSIFIER.detectMultiScale(gray, minNeighbors=4, minSize=(100,100))
    for (x,y,w,h) in faces:
        if debug:
            cv2.rectangle(image, (x,y), (x+w,y+h), (255,190,190), 2)
            cv2.putText(image, name, (x+5,y-5), cv2.FONT_HERSHEY_SIMPLEX, 1, (255,190,190), 2)
    if (len(faces) > 0):
        face = gray[y:y+h, x:x+w]
        folder = "%s/%s"%(rootfolder,name)
        os.makedirs(folder, exist_ok=True)
        count = len(os.listdir(folder))
        cv2.imwrite("%s/%d.jpg"%(folder,count), face)

    return image

def make_mod_extractface(name, rootfolder="faces", debug=False): # Factory method
    return lambda img: mod_extractface(img, name, rootfolder, debug)

def extractfaces(image_fn, names, debug=True):
    """ Extract faces from an image file, re-using 'mod_extractface()' """
    image = cv2.imread(image_fn)
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    faces = FACE_CLASSIFIER.detectMultiScale(gray, minNeighbors=10, minSize=(100,100))
    for (x,y,w,h) in faces:
        mod_extractface(image[y:y+h, x:x+w], names.pop(0), debug=debug)
    if debug:
        cv2.imshow("extractfaces",image)
        cv2.waitKey(1)    

def name2id(name):
    return np.sum([ord(c) for c in name]) & 0xff

def id2name(id, rootfolder="faces"):
    for name in os.listdir(rootfolder):
        if (name2id(name) == id):
            return name
    return None

def train_faces(rootfolder="faces", debug=False):
    """ Re-train the recognizer '{rootfolder}.yml' on all images in the root folder.
        While debugging, press ESCAPE key to stop loading more faces for a given name.
    """
    recognizer = cv2.face.LBPHFaceRecognizer_create()
    
    faces = []; names = []
    for name in os.listdir(rootfolder):
        if (name[0] == "."): continue
        folder = "%s/%s"%(rootfolder,name)
        for fn in os.listdir(folder): # Load faces for current name
            if (fn[0] == "."): continue
            face = Image.open("%s/%s"%(folder,fn)).convert('L')
            face = cv2.resize(np.array(face,'uint8'),  (200,200))
            faces.append(face) # Best performance if we train on identical sizes
            names.append(name)
            if debug:
                cv2.imshow(name, face)
                if (cv2.waitKey(300) == 27): # ESCAPE key pressed
                    break # Stop loading faces for current name
    if debug:
        for name in names:
            cv2.destroyWindow(name)
        cv2.waitKey(1)
    recognizer.train(faces, labels=np.asarray([name2id(n) for n in names],"uint"))
    recognizer.write(rootfolder+".yml")


def mod_detectface(image, recognizer):
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    faces = FACE_CLASSIFIER.detectMultiScale(gray, minNeighbors=4, minSize=(100,100))
    for (x,y,w,h) in faces:
        face = cv2.resize(gray[y:y+h, x:x+w], (200,200)) # Best performance if we match on same or smaller sizes than trained on? 
        id, confidence = recognizer.predict(face)
        name = id2name(id)
        # Annotate the image
        cv2.rectangle(image, (x,y), (x+w,y+h), (255,190,190), 2)
        if (name is not None):
            cv2.putText(image, name, (x+5,y-5), cv2.FONT_HERSHEY_SIMPLEX, 1, (255,190,190), 2)
            ci = "%.f%%"%(confidence)
            cv2.putText(image, ci, (x+5,y+h-5), cv2.FONT_HERSHEY_SIMPLEX, 1, (255,190,190), 1)

    return image

def make_mod_detectface(filter="faces.yml"): # Factory method
    recognizer = cv2.face.LBPHFaceRecognizer_create()
    recognizer.read(filter)
    return lambda img: mod_detectface(img, recognizer=recognizer)



if __name__ == "__main__":
    train_faces()
    show_video(1000, make_mod_detectface(), close=False)