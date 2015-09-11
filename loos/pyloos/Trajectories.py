# (c) 2015 Tod D. Romo, Grossfield Lab, URMC
import loos
import copy

class Trajectory(object):
    """
    This class turns a loos Trajectory into something more
    python-like.  Behind the scenes, it wraps a loos::AtomicGroup and
    a loos::Trajectory.  Remember that all atoms are shared.


    Examples:
        model = loos.createSystem('foo.pdb')
        traj = loos.pyloos.Trajectory('foo.dcd', model)
        calphas = loos.selectAtoms(model, 'name == "CA"')
        for frame in traj:
            print calphas.centroid()



        # The same thing but skipping the first 50 frames
        # and taking every other frame
        traj = loos.pyloos.Trajectory('foo.dcd', model, skip=50, stride=2)

        # Only use frames 19-39 (i.e. the 20th through 40th frames)
        traj = loos.pyloos.Trajectory('foo.dcd', model, iterator=range(19,40))

        # An alternative way of only iterating over a subset...
        model = loos.createSystem('foo.pdb')
        traj = loos.pyloos.Trajectory('foo.dcd', model, subset='name == "CA"')


    """

    def __init__(self, fname, model, **kwargs):
        """
        Instantiate a Trajectory object.  Takes a filename and an
        AtomicGroup model.

        keyword arguments:
            skip -- # of frames to skip from the start
          stride -- # of frames to step through by
        iterator -- Python iterator used to pick frames
                    (overrides skip and stride)
          subset -- Use this selection as the subset to return for frames...
        """
        skip = 0
        stride = 1
        iterator = None

        if 'skip' in kwargs:
            skip = kwargs['skip']
        if 'stride' in kwargs:
            stride = kwargs['stride']
        if 'iterator' in kwargs:
            iterator = kwargs['iterator']
        if 'subset' in kwargs:
            self.subset = loos.selectAtoms(model, kwargs['subset'])
        else:
            self.subset = model
            
        self.model = model
        self.fname = fname
        self.traj = loos.createTrajectory(fname, model)

        self.framelist = []
        if iterator is None:
            it = range(skip, self.traj.nframes(), stride)
        else:
            it = iter(iterator)
        
        for i in it:
            self.framelist.append(i)

        self.index = 0

    def fileName(self):
        """
        Return the filename that this Trajectory represents
        """
        return(self.fname)
        
    def setSubset(self, selection):
        """
        Set the subset used when iterating over a trajectory.
        The selection is a LOOS selection string.
        """
        self.subset = loos.selectAtoms(self.model, selection)

        
    def __iter__(self):
        self.index = 0
        return(self)

    def __len__(self):
        """
        Number of frames in the trajectory
        """
        return(len(self.framelist))


    def reset(self):
        """Reset the iterator"""
        self.index = 0

    def next(self):
        if (self.index >= len(self.framelist)):
            raise StopIteration
        frame = self.__getitem__(self.index)

        self.index += 1
        return(frame)


    def trajectory(self):
        """Access the wrapped loos.Trajectory"""
        return(self.traj)

    
    def readFrame(self, i):
        """Read a frame and update the model"""
        if (i < 0 or i >= len(self.framelist)):
            raise IndexError
        self.traj.readFrame(self.framelist[i])
        self.traj.updateGroupCoords(self.model)
        return(self.subset)

    def currentFrame(self):
        """Return the current frame (subset)"""
        return(self.subset)

    def currentModel(self):
        """Return the current model"""
        return(self.model)
    
    def currentRealIndex(self):
        """The 'real' frame in the trajectory for this index"""
        return(self.framelist[self.index-1])

    def currentIndex(self):
        """The state of the iterator"""
        return(self.index-1)


    def frameNumber(self, i):
        """
        Returns the real frame numbers corresponding to the passed indices.  Can accept
        either an integer or a list of integers.

        For example,
            t = loos.pyloos.Trajectory('foo.dcd', model, skip=50)

            t.frameNumber(0)           == 50
            t.frameNumber(range(0,2))  == [50,51]
        """
        if type(i) is int:
            if (i < 0):
                i += len(self.framelist)
            return(self.framelist[i])
        
        indices = [x if x >=0 else len(self.framelist)+x for x in i]
        framenos = [self.framelist[x] for x in indices]
        return(framenos)


    def getSlice(self, s):
        indices = list(range(*s.indices(self.__len__())))
        ensemble = []
        for i in indices:
            self.traj.readFrame(self.framelist[i])
            self.traj.updateGroupCoords(self.model)
            dup = self.subset.copy()
            ensemble.append(dup)
        return(ensemble)


    def __getitem__(self, i):
        """Handle array indexing and slicing.  Negative indices are
        relative to the end of the trajectory"""
        if isinstance(i, slice):
            return(self.getSlice(i))

        if (i < 0):
            i += len(self.framelist)
        if (i >= len(self.framelist) or i < 0):
            raise IndexError
        self.traj.readFrame(self.framelist[i])
        self.traj.updateGroupCoords(self.model)
        return(self.subset)



class VirtualTrajectory(object):
    """
    This class can combine multiple loos.pyloos.Trajectory objects
    into one big "virtual" trajectory.  Any skips or strides set in
    the contained trajectories will be honored.  In addition, a skip
    and a stride for the whole meta-trajectory are available.

    There is no requirement that the subsets used for all trajectories
    must be the same.  Ideally, the frame (subset) that is returned
    should be compatible (e.g. same atoms in the same order), but the
    models used for each trajectory (and the corresponding subset
    selection) can vary.  Why would you want to do this?  Imagine
    combining three different GPCRs where the subsets are the common
    trans-membrane C-alphas.  This makes processing all of the
    ensembles together easier.

    Since each contained trajectory can have a different set of shared
    atoms it updates, care must be taken when pre-selecting atoms.

    Examples:
      model = loos.createSystem('foo.pdb')
      traj1 = loos.pyloos.Trajectory('foo-1.dcd', model)
      traj2 = loos.pyloos.Trajectory('foo-2.dcd', model)
      vtraj = loos.pyloos.VirtualTrajectory(traj1, traj2)

      for frame in vtraj:
          print frame.centroid()


      # Adding another trajectory in with its own stride and skip
      traj3 = loos.pyloos.Trajectory('foo-3.dcd', skip=50, stride=2)
      vtraj.append(traj3)

      # Same as above but stride through the combined trajectory
      vtraj10 = loos.pyloos.VirtualTrajectory(traj1, traj2, stride=10)

    

    """
    def __init__(self, *trajs, **kwargs):
        """
        Instantiate a VirtualTrajectory object.

        Keyword arguments:
            skip -- # of frames to skip at start of composite traj
          stride -- # of frames to step through in the composite traj
        iterator -- Python iterator used to pick frames from the composite traj

        """

        self.skip = 0
        self.stride = 1
        self.nframes = 0
        self.iterator = None
        self.trajectories = list(trajs)

        self.index = 0
        self.framelist = []
        self.trajlist = [] 
        self.stale = 1

        if 'skip' in kwargs:
            self.skip = kwargs['skip']
        if 'stride' in kwargs:
            self.stride = kwargs['stride']
        if 'iterator' in kwargs:
            self.iterator = kwargs['iterator']
        if 'subset' in kwargs:
            self.setSubset(kwargs['subset'])

        
    def append(self, *traj):
        """
        Add a trajectory to the end of the virtual trajectory.  Resets
        the iterator state
        """
        self.trajectories.extend(traj)
        self.stale = 1


    def setSubset(self, selection):
        """
        Set the subset selection for all managed trajectories
        """
        for t in self.trajectories:
            t.setSubset(selection)

    def currentFrame(self):
        """
        Return the current frame/model.  If the iterator is past the
        end of the trajectory list, return the last valid frame.
        """
        if self.stale:
            self.initFrameList()

        if self.index >= len(self.framelist):
            i = len(self.framelist) - 1
        else:
            i = self.index
            
        return(self.trajectories[self.trajlist[i]].currentFrame())

    def currentIndex(self):
        """
        Return index into composite trajectory for current frame
        """
        return(self.index-1)
    

    def currentTrajectoryIndex(self):
        """
        Returns the index into the list of trajectories that the
        current frame is from
        """
        return(self.trajlist[self.index-1])

    def currentTrajectory(self):
        """
        Returns the loos.pyloos.Trajectory object that the current
        frame is from
        """
        return(self.trajectories[self.trajlist[self.index-1]])

    def frameLocation(self, i):
        """
        Returns a tuple containing information about where a frame in the virtual trajectory 
        comes from.

        (frame-index, traj-index, trajectory, real-frame-within-trajectory)

        Consider the following:
          t1 = loos.pyloos.Trajectory('foo.dcd', model)   # 50 frames
          t2 = loos.pyloos.Trajectory('bar.dcd', model)   # 25 frames
          vt = loos.pyloos.VirtualTrajectory(t1, t2)

        The frame-index is the index into the corresponding trajectory object.  For example,
        frameLocation(50) would have a frame-index of 0 because vt[50] would return the first
        frame from t2.

        The traj-index is the index into the list of managed trajectories for the frame.
        In the above example, the traj-index will be 1.

        The trajectory is the actual loos.pyloos.Trajectory object that contains the frame.

        The real-frame-within-trajectory is the same as calling trajectory.frameNumber(frame-index).
        Instead of the t1 above, imagine it was setup this way,
          t1 = loos.pyloos.Trajectory('foo.dcd', model, skip=25)
        Now, vt.frameLocation(0) will return (0, 0, t1, 25),
        and vt.frameLocation(25) will return (25, 1, t2, 0)
        
        """
        if (self.stale):
            self.initFrameList()
            
        if (i < 0):
            i += len(self.framelist)

        t = self.trajectories[self.trajlist[i]]
        return( self.framelist[i], self.trajlist[i], t, t.frameNumber(self.framelist[i]))
    
    def initFrameList(self):
        frames = []
        trajs = []
        for j in range(len(self.trajectories)):
            t = self.trajectories[j]
            for i in range(len(t)):
                frames.append(i)
                trajs.append(j)

        self.framelist = []
        self.trajlist = []
        n = len(frames)
        if (self.iterator is None):
            it = iter(range(self.skip, n, self.stride))
        else:
            it = iter(iterator)
        for i in it:
            self.framelist.append(frames[i])
            self.trajlist.append(trajs[i])

        self.index = 0
        self.stale = 0
            
    def __len__(self):
        """
        Total number of frames
        """
        if self.stale:
            self.initFrameList()
        return(len(self.framelist))

                
    def __getitem__(self, i):
        """
        Return the ith frame in the composite trajectory.  Supports
        Python slicing.  Negative indices are relative to the end of
        the composite trajectory.
        """
        if self.stale:
            self.initFrameList()

        if isinstance(i, slice):
            return(self.getSlice(i))

        if (i < 0):
            i += len(self)
        if (i >= len(self)):
            raise IndexError

        return(self.trajectories[self.trajlist[i]][self.framelist[i]])


    def __iter__(self):
        if self.stale:
            self.initFrameList()
        self.index = 0
        return(self)

    def reset(self):
        self.index = 0

    def next(self):
        if self.stale:
            self.initFrameList()
        if (self.index >= len(self.framelist)):
            raise StopIteration
        frame = self.__getitem__(self.index)
        self.index += 1
        return(frame)

    def getSlice(self, s):
        indices = list(range(*s.indices(self.__len__())))
        ensemble = []
        for i in indices:
            frame = self.trajectories[self.trajlist[i]][self.framelist[i]].copy()
            ensemble.append(frame)
        return(ensemble)




class AlignedVirtualTrajectory(VirtualTrajectory):
    """
    A virtual trajectory that supports iterative alignment.  Only the
    transformation needed to align each frame is stored.  When a frame
    is accessed, it is automatically transformed into the aligned
    orientation.  The selection used for aligning can be set with the
    'alignwith' argument to the constructor (or the alignWith()
    method).

    In order to do the alignment, the alignwith subset must be read
    into memory and temporarily stored.  This can potentially use a
    lot of memory and create delays in execution.  Once the alignment
    is complete, however, those cached frames are released and
    subsequent frame accesses will be quick.

    See VirtualTrajectory for some basic examples in addition to
    below:

    # Align using only C-alphas (the default)
    vtraj = loos.pyloos.AlignedVirtualTrajectory(traj1, traj2)

    # Align using only backbone atoms
    vtraj = loos.pyloos.AlignedVirtualTrajectory(traj1, traj2, alignwith='name =~ "^(C|N|O|CA)$"')

    # Add another trajectory
    vtraj.append(traj3)
    """

    def __init__(self, *trajs, **kwargs):
        super(AlignedVirtualTrajectory, self).__init__(*trajs, **kwargs)
        self.aligned = False
        self.xformlist = []
        self.rmsd = 0
        self.iters = 0
        if 'alignwith' in kwargs:
            self.alignwith = kwargs['alignwith']
        else:
            self.alignwith = 'name == "CA"'

        if 'reference' in kwargs:
            self.reference = copy.deepcopy(kwargs['reference'])
        else:
            self.reference = None


    def append(self, *traj):
        """
        Add another trajectory at the end.  Requires re-aligning
        """
        self.trajectories.extend(traj)
        self.stale = True
        self.aligned = False

    def alignWith(self, selection):
        """
        Change the selection used to align with.  Requires re-aligning
        """
        self.alignwith = selection
        self.aligned = False


    def __iter__(self):
        self.align()
        self.index = 0
        return(self)


    def setReference(self, reference):
        self.reference = copy.deepcopy(reference)
        self.aligned = False
    
    def align(self):
        """
        Align the frames (called implicitly on iterator or array access)
        """
        current_traj = None
        current_subset = None
        ensemble = []

        if self.stale:
            self.initFrameList()

        if self.reference:
            self.xformlist = []
            for i in range(len(self.framelist)):
                t = self.trajectories[self.trajlist[i]]
                if t != current_traj:
                    current_traj = t
                    current_subset = loos.selectAtoms(t.currentModel(), self.alignwith)
                t.readFrame(self.framelist[i])
                m = current_subset.superposition(self.reference)
                x = loos.XForm()
                x.load(m)
                self.xformlist.append(x)

            self.rmsd = 0.0
            self.iters = 0

        else:
            
            for i in range(len(self.framelist)):
                t = self.trajectories[self.trajlist[i]]
                if t != current_traj:
                    current_traj = t
                    current_subset = loos.selectAtoms(t.currentModel(), self.alignwith)
                t.readFrame(self.framelist[i])
                ensemble.append(current_subset.copy())

            (self.xformlist, self.rmsd, self.iters) = loos.iterativeAlignEnsemble(ensemble)

        self.aligned = True

        
    def getSlice(self, s):
        indices = list(range(*s.indices(self.__len__())))
        ensemble = []
        for i in indices:
            frame = self.trajectories[self.trajlist[i]][self.framelist[i]].copy()
            frame.applyTransform(self.xformlist[i])
            ensemble.append(frame)
        return(ensemble)

        
    def __getitem__(self, i):
        """
        Returns the ith frame aligned.  Supports Python slices.  Negative indices are relative
        to the end of the composite trajectory.
        """
        if not self.aligned:
            self.align()

        if isinstance(i, slice):
            return(self.getSlice(i))
        
        if (i < 0):
            i += len(self.framelist)
        if (i >= len(self.framelist)):
            raise IndexError

        frame = self.trajectories[self.trajlist[i]][self.framelist[i]]
        frame.applyTransform(self.xformlist[i])
        return(frame)
