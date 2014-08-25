VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "Researcher"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Public Name
Public Rac
Dim CPIDS() As cpid
Public CPIDCount As Integer
Public TotalProjects As Integer
Public Balance As Currency
Public Balance2 As Currency
Public PaymentCount As Long
Public PaymentCount2 As Long
Public ResearcherType As String
Public iResearcherType As Long
Public ComputingPowerCores As Double

Public Function GetCPIDs() As cpid()
   GetCPIDs = CPIDS
End Function

Public Function AddCPID(ByRef r_cpid As cpid)
ReDim Preserve CPIDS(CPIDCount)
Set CPIDS(CPIDCount) = r_cpid
CPIDCount = CPIDCount + 1
End Function
Public Function GetCPID(iCPIDNumber As Integer) As cpid
Set GetCPID = CPIDS(iCPIDNumber)
End Function
Public Sub AddUserProject(CPIDNumber As Integer, TheProject As Project)
TotalProjects = TotalProjects + 1
Call CPIDS(CPIDNumber).AddProject(TheProject)
End Sub
Public Sub ReplaceUserProject(CPIDNumber As Integer, TheProject As Project, iPosition As Long)
Call CPIDS(CPIDNumber).PutProject(Val(iPosition), TheProject)
End Sub

Public Sub AddToBalance(Amount As Currency)
Balance = Balance + Amount
PaymentCount = PaymentCount + 1
End Sub

Public Sub AddToBalance2(Amount As Currency)
Balance2 = Balance2 + Amount
PaymentCount2 = PaymentCount2 + 1

End Sub
