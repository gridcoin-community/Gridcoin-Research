Attribute VB_Name = "modSimulation"
Public Const cobblestones_per_core   As Long = 2000
Public BoincProjects(23) As Project
Public Researchers(100) As Researcher
Public iResearcherCount As Integer
Public BoincProjectCount As Integer
Public CompensationByType(10) As Currency
Public CompensationCtByType(10) As Double
Public CompensationByType2(10) As Currency
Public CompensationCtByType2(10) As Double
Public ResearcherType(10) As String

Public Sub Simulate()
'Research for One Year:
Dim iWk As Long
Dim cobblestones_crunched As Double
Dim crunched_per_project As Double
Dim CurrentProject As Project
Dim iDay As Integer
Dim theDate As Date
For iDay = 1 To 365

    theDate = DateAdd("d", iDay, CDate("1-1-2013"))
    'For each Researcher CPID, simulate BOINC daily participation by incrementing USER RAC
    For iResearcher = 0 To iResearcherCount - 1
        Dim project_cobblestone_factor As Long
        cobblestones_crunched = Researchers(iResearcher).ComputingPowerCores * cobblestones_per_core
        crunched_per_project = cobblestones_crunched / (Researchers(iResearcher).TotalProjects + 0.01)
        For iCPIDCount = 0 To Researchers(iResearcher).CPIDCount - 1
            'For each project in the cpid
            Dim CurrentCPID As CPID
            Set CurrentCPID = Researchers(iResearcher).GetCPID(Val(iCPIDCount))
            For iProjectCount = 0 To CurrentCPID.ProjectCount - 1
                Call CurrentCPID.GetProject2(Val(iProjectCount), CurrentProject)
                'Increment RAC for this project
                crunched = crunched_per_project * CurrentProject.CobblestoneMultiplier
                Call CurrentProject.AddUserRac(Val(crunched), Val(iDay))
                Call CurrentProject.AddNetworkRac(Val(crunched), Val(iDay))
                CurrentProject.Purge30DayNetworkDMA (iDay)
                CurrentProject.Purge30DayUserDMA (iDay)
            Next iProjectCount
        Next iCPIDCount
        'Award a block
        myMag = GetMagnitude(Researchers(iResearcher))
        MyPayment = 1 * myMag
        
        Researchers(iResearcher).AddToBalance (MyPayment)
        
        Dim myType As Integer
        myType = Researchers(iResearcher).iResearcherType
        If myMag > 0 Then
            CompensationByType(myType) = CompensationByType(myType) + Val(MyPayment)
            CompensationCtByType(myType) = CompensationCtByType(myType) + 1
        End If
        'Add Mag Type 2 Payments
        MyMagVersion2 = GetMagnitude2(Researchers(iResearcher))
        MyPayment = 1 * MyMagVersion2
        Researchers(iResearcher).AddToBalance2 (MyPayment)
        
        If MyMagVersion2 > 0 Then
            CompensationByType2(myType) = CompensationByType2(myType) + Val(MyPayment)
            CompensationCtByType2(myType) = CompensationCtByType2(myType) + 1
        End If
        If myType = 2 Then
          '  Debug.Print iResearcher, Researchers(iResearcher).Balance, myMag, CompensationByType(myType), CompensationCtByType(myType)
        End If
        If Researchers(iResearcher).ResearcherType = "HUNTER" And (iDay Mod 14) = 0 Then
                Call AttachAndDetach(Val(iResearcher))
        End If
    Next iResearcher
Next iDay
'Write results to file
Dim sOut As String
Open "c:\BoincMonteCarlo.txt" For Output As #1
AppendResult "Projects and Total Credits:" + vbCrLf


For i = 0 To 23
    Dim sRow As String
    sOut = BoincProjects(i).Name + ", Network RAC: " + RoundToString(BoincProjects(i).NetworkRAC(), False) + ", AvgRac: " + RoundToString(BoincProjects(i).Average, False)
    AppendResult sOut
    
Next i

'Enumerate Through the researchers and Find Mag Levels
AppendResult vbCrLf + "Individual Researcher Results:" + vbCrLf
For i = 0 To iResearcherCount - 1
    Dim cAvg As Double
    sOut = Researchers(i).Name + ": Magnitude1: " + RoundToString(GetMagnitude(Researchers(i)), False) _
        + ", Magnitude2: " + RoundToString(GetMagnitude2(Researchers(i)), False) _
        + ", Mag1Balance:  " + RoundToString(Researchers(i).Balance, True) + "," _
        + "Mag1AvgReward: " + RoundToString(Researchers(i).Balance / Researchers(i).PaymentCount, True) _
        + ", Mag2AvgReward: " + RoundToString(Researchers(i).Balance2 / (Researchers(i).PaymentCount2 + 0.01), True)
    AppendResult sOut
Next i
AppendResult vbCrLf + "Global Results by Researcher Type:" + vbCrLf
For i = 0 To 4
    cAvg = CompensationByType(i) / (CompensationCtByType(i) + 0.01)
    cAvg2 = CompensationByType2(i) / (CompensationCtByType2(i) + 0.01)
    sOut = "Type " & Trim(i) & " " + Trim(ResearcherType(i)) + ": Mag1Avg: " + RoundToString(cAvg, 2) _
    + ", Mag2Avg: " + RoundToString(cAvg2, 2)
    AppendResult sOut
Next i
Close

End

End Sub
Public Function AppendResult(data As String)
Print #1, data
Debug.Print data
End Function
Public Function RoundToString(data As Variant, bCur As Boolean) As String
If Not bCur Then
    RoundToString = Trim(Round(data, 2))
    
Else
    RoundToString = Format(Trim(Round(data, 2)), "$###########.00")
End If
End Function
Public Function SeekOutLowestRACProject()
Dim hiRac As Double
hiRac = 99999999
Dim id As Long
For x = 0 To BoincProjectCount - 1
    Dim Rac As Double
    Rac = BoincProjects(x).GetNetworkRac() / BoincProjects(x).CobblestoneMultiplier
    If Rac < hiRac And Rac > 0 Then
        hiRac = Rac: id = x
    End If
Next x
'Debug.Print BoincProjects(id).bNonPopular, Rac
SeekOutLowestRACProject = id
End Function

Public Function GetMagnitude2(ByRef r As Researcher) As Double
Dim Entries As Long
Dim TotalMag As Long
Dim CurrentProject As Project
Dim bUsedProjects(30) As Boolean
For iCPIDCount = 0 To r.CPIDCount - 1
          'For each project in the cpid
           Dim CurrentCPID As CPID
           Set CurrentCPID = r.GetCPID(Val(iCPIDCount))
           For iProjectCount = 0 To CurrentCPID.ProjectCount - 1
               Call CurrentCPID.GetProject2(Val(iProjectCount), CurrentProject)
               Dim projAvg As Double
               MyAvg = (CurrentProject.GetUserAverage / (BoincProjects(CurrentProject.id).Average + 0.01))
               If (CurrentProject.GetUserTotalRac > 100 And BoincProjects(CurrentProject.id).GetNetworkRac > 1) Then
                    Entries = Entries + 1
                    TotalMag = TotalMag + MyAvg
               End If
               bUsedProjects(CurrentProject.id) = True
           Next iProjectCount
Next iCPIDCount
'For all projects user has NOT participated in:
For xProjects = 0 To BoincProjectCount - 1
    If bUsedProjects(xProjects) = False Then
         'Weigh user down for non-participating projects
         Entries = Entries + 1
         TotalMag = TotalMag + 0
    End If
Next xProjects
Dim MagOut As Double
MagOut = (TotalMag / (Entries + 0.01)) * 100
If MagOut > 500 Then MagOut = 500
GetMagnitude2 = MagOut
End Function
Public Function GetMagnitude(ByRef r As Researcher) As Double
Dim Entries As Long
Dim TotalMag As Long
Dim CurrentProject As Project
For iCPIDCount = 0 To r.CPIDCount - 1
          'For each project in the cpid
           Dim CurrentCPID As CPID
           Set CurrentCPID = r.GetCPID(Val(iCPIDCount))
           For iProjectCount = 0 To CurrentCPID.ProjectCount - 1
               Call CurrentCPID.GetProject2(Val(iProjectCount), CurrentProject)
               Dim projAvg As Double
               MyAvg = (CurrentProject.GetUserAverage / (BoincProjects(CurrentProject.id).Average + 0.01))
               If r.ResearcherType = "HUNTER" Then
                        '                    Debug.Print CurrentProject.bNonPopular, CurrentProject.GetNetworkRac
               End If
               If (CurrentProject.GetUserTotalRac > 100 And BoincProjects(CurrentProject.id).GetNetworkRac > 1) Then
                    Entries = Entries + 1
                    TotalMag = TotalMag + MyAvg
               End If
           Next iProjectCount
Next iCPIDCount
Dim MagOut As Double
MagOut = (TotalMag / (Entries + 0.01)) * 100
If MagOut > 500 Then MagOut = 500
GetMagnitude = MagOut
End Function
Public Sub AttachAndDetach(iResNo As Integer)
Dim iLowestRACProject As Integer
iLowestRACProject = SeekOutLowestRACProject()
If IsParticipating(Val(iLowestRACProject)) = True Then Exit Sub 'Already Participating in the lowest rac project as a hunter
            Dim p As Project
            Set p = New Project
            p.Name = BoincProjects(iLowestRACProject).Name
            p.URL = BoincProjects(iLowestRACProject).URL
            p.CobblestoneMultiplier = BoincProjects(iLowestRACProject).CobblestoneMultiplier
            p.id = iLowestRACProject
            p.bNonPopular = BoincProjects(iLowestRACProject).bNonPopular
            Call Researchers(iResNo).AddUserProject(0, p)
            '  Call Researchers(iResNo).ReplaceUserProject(0, p, 0)
End Sub
Public Function IsParticipating(iProj As Double) As Boolean
Dim CurrentProject As Project
For x = 0 To iResearcherCount - 1
      For iCPIDCount = 0 To Researchers(iResearcher).CPIDCount - 1
          'For each project in the cpid
           Dim CurrentCPID As CPID
           Set CurrentCPID = Researchers(iResearcher).GetCPID(Val(iCPIDCount))
           For iProjectCount = 0 To CurrentCPID.ProjectCount - 1
               Call CurrentCPID.GetProject2(Val(iProjectCount), CurrentProject)
               If CurrentProject.id = iProj Then IsParticipating = True: Exit Function
           Next iProjectCount
        Next iCPIDCount
Next x
IsParticipating = False
End Function
