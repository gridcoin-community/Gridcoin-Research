VERSION 5.00
Begin VB.Form frmMonteCarlo 
   Caption         =   "Boinc Monte Carlo Simulation"
   ClientHeight    =   3030
   ClientLeft      =   120
   ClientTop       =   450
   ClientWidth     =   4560
   LinkTopic       =   "Form1"
   ScaleHeight     =   3030
   ScaleWidth      =   4560
   StartUpPosition =   3  'Windows Default
End
Attribute VB_Name = "frmMonteCarlo"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Form_Load()
'Initialize Projects
Dim boinc_projects(23) As String
boinc_projects(0) = "http://boinc.bakerlab.org/rosetta/   |rosetta@home|1"
boinc_projects(1) = "http://docking.cis.udel.edu/         |Docking|2"
boinc_projects(2) = "http://www.malariacontrol.net/       |malariacontrol.net|3"
boinc_projects(3) = "http://www.worldcommunitygrid.org/   |World Community Grid|4"
boinc_projects(4) = "http://asteroidsathome.net/boinc/    |Asteroids@home|5"
boinc_projects(5) = "http://setiathome.berkeley.edu/      |SETI@home|6"
boinc_projects(6) = "http://milkyway.cs.rpi.edu/milkyway/|Milkyway@Home|57"
boinc_projects(7) = "http://aerospaceresearch.net/constellation/|Constellation|8"
boinc_projects(8) = "http://www.cosmologyathome.org/     |Cosmology@Home|9"
boinc_projects(9) = "http://einstein.phys.uwm.edu/       |Einstein@Home|10"
boinc_projects(10) = "http://www.enigmaathome.net/        |Enigma@Home|11"
boinc_projects(11) = "http://registro.ibercivis.es/       |ibercivis|12"
boinc_projects(12) = "http://lhcathomeclassic.cern.ch/sixtrack/|LHC@home 1.0|1"
boinc_projects(13) = "http://lhcathome2.cern.ch/test4theory|Test4Theory@Home|2"
boinc_projects(14) = "http://mindmodeling.org/            |MindModeling@Beta|2"
boinc_projects(15) = "http://escatter11.fullerton.edu/nfs/|NFS@Home|3"
boinc_projects(16) = "http://numberfields.asu.edu/NumberFields/|NumberFields@home|3"
boinc_projects(17) = "http://oproject.info/               |OProject@Home|3"
boinc_projects(18) = "http://www.primegrid.com/           |PrimeGrid|3"
boinc_projects(19) = "http://sat.isa.ru/pdsat/            |SAT@home|3"
boinc_projects(20) = "http://boincsimap.org/boincsimap/   |simap|3"
boinc_projects(21) = "http://boinc.thesonntags.com/collatz/|Collatz Conjecture|3"
boinc_projects(22) = "http://mmgboinc.unimi.it/           |SimOne@home|4"
boinc_projects(23) = "http://volunteer.cs.und.edu/subset_sum/|SubsetSum@Home|5"
      
Dim vProj() As String
Randomize
Dim iProjCt As Integer
Dim lNonPopularProjects As Long
    
For iProjectNumber = 0 To 23
    vProj = Split(boinc_projects(iProjectNumber), "|")
    Set BoincProjects(iProjectNumber) = New Project
    BoincProjects(iProjectNumber).URL = vProj(0)
    BoincProjects(iProjectNumber).Name = vProj(1)
    BoincProjects(iProjectNumber).CobblestoneMultiplier = Val(vProj(2))
    lNonPopularProjects = Rn(100)
    If lNonPopularProjects < 10 Then BoincProjects(iProjectNumber).bNonPopular = True
Next iProjectNumber

BoincProjectCount = iProjectNumber
Dim iType As Long

ResearcherType(0) = "NORMAL"
ResearcherType(1) = "HUNTER"
ResearcherType(2) = "LOTS_OF_CPIDS"
ResearcherType(3) = "ONE_CPID"
ResearcherType(4) = "POWER_USER"

Dim iResNo As Integer
For iResNo = 0 To 99
    Set Researchers(iResNo) = New Researcher
    iType = Rn(4)
    iHunterChance = Rn(100)
    If iHunterChance > 15 And iType = 1 Then iType = 0
    iLotsOfCpidsChance = Rn(100)
    If iLotsOfCpidsChance > 15 And iType = 2 Then iType = 0
    iPowerUserChance = Rn(100)
    If iPowerUserChance > 20 And iType = 4 Then iType = 0
    
    sType = ResearcherType(iType)
    Researchers(iResNo).iResearcherType = iType
    Researchers(iResNo).Name = "RS_" + Trim(sType) + "#" + Trim(iResNo)
    Researchers(iResNo).ResearcherType = sType
    Researchers(iResNo).ComputingPowerCores = 4
    If iType = 4 Then Researchers(iResNo).ComputingPowerCores = 40
    'Assign 1-50 CPIDs and Magnitudes
    Select Case iType
       Case 0
            iCPIDCount = Rn(1) + 1
       Case 1
            iCPIDCount = Rn(1) + 1
       Case 2
            iCPIDCount = Rn(45) + 1
       Case 3
            iCPIDCount = 1
    End Select
   
    For iCPIDs = 0 To iCPIDCount - 1
        Dim r_cpid As CPID
        Set r_cpid = New CPID
        r_cpid.sCpid = Hex(Rnd(1) * 1000000)
        Call Researchers(iResNo).AddCPID(r_cpid)
        
        'Add Projects to the cpid
        iProjCt = Rn(4) + 1
        
        
        For iProjs = 0 To iProjCt - 1
        
            Dim theProj As Integer
            Dim PopularProject As Boolean
            
GetAnotherProject:
            theProj = Rn(Val(BoincProjectCount - 1))
            If BoincProjects(theProj).bNonPopular = True Then GoTo GetAnotherProject
            
            Dim p As Project
            Set p = New Project
            p.Name = BoincProjects(theProj).Name
            p.URL = BoincProjects(theProj).URL
            p.CobblestoneMultiplier = BoincProjects(theProj).CobblestoneMultiplier
            p.id = theProj
            Call Researchers(iResNo).AddUserProject(Val(iCPIDs), p)
        Next iProjs
      
    Next iCPIDs
    
Next iResNo

    iResearcherCount = iResNo
    
    Call Simulate
    
End Sub

Public Function Rn(iCeiling As Long) As Double
Dim iOut As Double
iOut = Rnd(1) * iCeiling
Rn = iOut
End Function
