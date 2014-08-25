VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "CPID"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Public sCpid As String
Public Magnitude As Double
Dim Projects() As Project
Public ProjectCount As Integer
Public Sub AddProject(TheProject As Project)
ReDim Preserve Projects(ProjectCount)
Set Projects(ProjectCount) = TheProject
ProjectCount = ProjectCount + 1
End Sub
Public Function GetProject(iProjectNumber As Integer) As Project
Set GetProject = Projects(iProjectNumber)
End Function
Public Sub GetProject2(iProjectNumber As Integer, ByRef Proj As Project)
Set Proj = Projects(iProjectNumber)
End Sub
Public Sub PutProject(iProjectNumber As Integer, ByRef Proj As Project)
Set Projects(iProjectNumber) = Proj
End Sub
