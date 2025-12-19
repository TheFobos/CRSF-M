import cv2
from cv2 import *
from abc import ABC, abstractmethod
import numpy as np
import time
from picamera2 import Picamera2

class AbstractVideoCapture(ABC):
    def __init__(self, parametrs):
        self.width = parametrs["width"]
        self.height = parametrs["height"]
        self.fps = parametrs["fps"]
        self.camera = None
        self.init()

    def __del__(self):
        pass
    
    @abstractmethod
    def init(self):
        pass
    
    @abstractmethod    
    def isOpened(self):
        return False
    
    @abstractmethod    
    def read(self):
        return False, None
    
    @abstractmethod    
    def release(selfe):
        pass

class VideoCaptureUSB(AbstractVideoCapture):
    def __init__(self, parametrs):
        AbstractVideoCapture.__init__(self, parametrs)

    def __del__(self):
        AbstractVideoCapture.__del__(self)

    def init(self):
        camera_index = self.__find_camera_index() # присвоение индекса камеры
        if camera_index != None:
            self.camera = cv2.VideoCapture(camera_index)
            self.camera.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
            self.camera.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
            self.camera.set(cv2.CAP_PROP_FPS, self.fps)

    def __find_camera_index(self): # функция подбора индекса USB камеры
        for index in range(10):  # Проверяем первые 10 индексов
            tmp_log_level = cv2.getLogLevel()
            cv2.setLogLevel(0)
            
            cap = cv2.VideoCapture(index)
            if cap.isOpened():
                ret, frame = cap.read()
                if ret:
                    cap.release()
                    cv2.setLogLevel(tmp_log_level)
                    return index
                
                cap.release()
                
        cv2.setLogLevel(tmp_log_level)
        return None

    def isOpened(self):
        return False
    
    def read(self):
        ret, frame = self.camera.read() 
        return ret, frame
    
    def release(self):
        self.camera.release()

class VideoCaptureFile(AbstractVideoCapture):
    def __init__(self, parametrs):
        self.path = parametrs["path"]
        AbstractVideoCapture.__init__(self, parametrs)
        
    def __del__(self):
        AbstractVideoCapture.__del__(self)

    def init(self):
        self.camera = cv2.VideoCapture(self.path)
        self.camera.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
        self.camera.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        self.camera.set(cv2.CAP_PROP_FPS, self.fps)

    def isOpened(self):
        return False
    
    def read(self):
        ret, frame = self.camera.read() 
        return ret, frame
    
    def release(self):
        self.camera.release()

class VideoCapturePI(AbstractVideoCapture):
    def __init__(self, parametrs):
        AbstractVideoCapture.__init__(self, parametrs)

    def __del__(self):
        AbstractVideoCapture.__del__(self)

    def init(self):
        self.camera = Picamera2()
        self.camera.configure(self.camera.create_video_configuration(main={"format": 'RGB888', "size": (self.width, self.height)},controls={"FrameRate": self.fps}))
        self.camera.start()
        time.sleep(0.5)

    def isOpened(self):
        return self.camera is not None and self.camera.started
    
    def read(self):
        frame = self.camera.capture_array() # считываем кадры с PI камеры
        ret = True
        return ret, frame
    
    def release(self):
        self.camera.stop()

class VideoCaptureIP(AbstractVideoCapture):
    def __init__(self, parametrs):
        self.ip = parametrs["ip"]
        AbstractVideoCapture.__init__(self, parametrs)

    def __del__(self):
        AbstractVideoCapture.__del__(self)

    def init(self):
        self.camera = cv2.VideoCapture(self.ip)
        self.camera.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
        self.camera.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        self.camera.set(cv2.CAP_PROP_FPS, self.fps)

    def isOpened(self):
        return False
    
    def read(self):
        ret, frame = self.camera.read()
        return ret, frame
    
    def release(self):
        self.camera.release()

class VideoCaptureFactory():
    
    def __init__(self):
        pass

    def __del__(self):
        pass

    def create(key, parametrs):
        if key == "VideoCaptureUSB":
            return VideoCaptureUSB(parametrs)
        elif key == "VideoCaptureFile":
            return VideoCaptureFile(parametrs)
        elif key == "VideoCapturePI":
            return VideoCapturePI(parametrs)
        elif key == "VideoCaptureIP":
            return VideoCaptureIP(parametrs)
