Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmDiagnostics
    Private msDiagnosticCPID As String = ""
    Private Sub frmDiagnostics_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        'Prepopulate the Diagnostic Test Name
        Dim sDiagName As String = "Verify Boinc Path,Find CPID,Verify CPID Is Valid,Verify CPID has RAC,Verify CPID is in Neural Network,Verify Wallet Is Synced,Verify Clock,Verify Addnode,Verify TCP 32749"
        Dim vDiagName As String() = Split(sDiagName, ",")
        dvDiag.Rows.Clear()
        dvDiag.Columns.Clear()
        dvDiag.Columns.Add("Diagnostic Name", "Diagnostic Name")
        dvDiag.Columns.Add("Diagnostic Result", "Diagnostic Result")
        dvDiag.Columns(0).Width = 175
        dvDiag.Columns(1).Width = 500
        For x As Integer = 0 To UBound(vDiagName)
            dvDiag.Rows.Add()
            dvDiag.Rows(x).Cells(0).Value = vDiagName(x)
        Next
    End Sub
    Private Sub btnRunDiagnostics_Click(sender As System.Object, e As System.EventArgs) Handles btnRunDiagnostics.Click
        Cursor.Current = Cursors.WaitCursor
        For x As Integer = 0 To dvDiag.Rows.Count - 1
            Dim sDiagnosticName As String = dvDiag.Rows(x).Cells(0).Value
            GuiDoEvents()
            System.Threading.Thread.Sleep(1000)
            dvDiag.Refresh()
            Select Case sDiagnosticName
                Case "Verify Boinc Path"
                    VerifyBoincPath(x)
                Case "Find CPID"
                    FindCPID(x)
                Case "Verify CPID Is Valid"
                    VerifyCPIDIsValid(x)
                Case "Verify CPID has RAC"
                    VerifyCPIDHasRac(x)
                Case "Verify CPID is in Neural Network"
                    VerifyCPIDIsInNeuralNetwork(x)
                Case "Verify Wallet Is Synced"
                    VerifyWalletIsSynced(x)
                Case "Verify Clock"
                    VerifyClock(x)
                Case "Verify Addnode"
                    VerifyAddNode(x)
                Case "Verify TCP 32749"
                    VerifyTCP32749(x)
            End Select
        Next
        Dim lErrorCount As Long = 0
        For x = 0 To dvDiag.Rows.Count - 2
            If Not dvDiag.Rows(x).Cells(1).Value Like "*PASSED*" And x <> 1 Then
                dvDiag.Rows(x).Cells(1).Style.BackColor = Drawing.Color.Red
                lErrorCount += 1
            End If
        Next
        dvDiag.Rows(dvDiag.Rows.Count - 1).Cells(0).Value = "Result"
        dvDiag.Rows(dvDiag.Rows.Count - 1).Cells(1).Value = IIf(lErrorCount = 0, "PASSED", "FAILED")
        If dvDiag.Rows(dvDiag.Rows.Count - 1).Cells(1).Value = "FAILED" Then dvDiag.Rows(dvDiag.Rows.Count - 1).Cells(1).Style.BackColor = Drawing.Color.Red

        Cursor.Current = Cursors.Default

    End Sub
    Private Sub VerifyCPIDIsInNeuralNetwork(iRow As Integer)
        'Check the wallet to see if they ever sent the beacon
        Dim sBeacons As String = ExtractXML(msSyncData, "<CPIDDATA>")
        If sBeacons.ToLower().Contains(LCase(msDiagnosticCPID)) Then
            dvDiag.Rows(iRow).Cells(1).Value = "PASSED"
        Else
            dvDiag.Rows(iRow).Cells(1).Value = "FAILED (execute advertisebeacon force)"
        End If
        If Len(sBeacons) < 100 Then
            dvDiag.Rows(iRow).Cells(1).Value = "BEACON DATA EMPTY, SYNC WALLET"
        End If
    End Sub
    Private Sub VerifyWalletIsSynced(iRow As Integer)
        Dim sWalletAge As String = ExtractXML(msSyncData, "<LASTBLOCKAGE>")
        Dim lWalletAge As Long = Val("0" + sWalletAge)
        If lWalletAge > (60 * 60) Then
            dvDiag.Rows(iRow).Cells(1).Value = "FAILED"
        Else
            dvDiag.Rows(iRow).Cells(1).Value = "PASSED"
        End If
        If Len(sWalletAge) = 0 Then
            dvDiag.Rows(iRow).Cells(1).Value = "SYNC DATA EMPTY"
        End If
    End Sub
    Private Sub VerifyCPIDHasRac(iRow As Integer)
        Dim sCPID As String = dvDiag.Rows(iRow - 2).Cells(1).Value
        Dim sError As String = ""
        Dim dTotalRac As Double = GetTotalCPIDRAC(sCPID, sError)
        If dTotalRac > 1 Then sError = "PASSED (" + Trim(dTotalRac) + ")" Else sError = "FAILED"
        dvDiag.Rows(iRow).Cells(1).Value = sError
    End Sub
    Private Sub VerifyAddNode(iRow As Integer)
        Dim sNarr As String = GetAddNodeNarrative(iRow)
        dvDiag.Rows(iRow).Cells(1).Value = sNarr
    End Sub
    Private Function GetAddNodeNarrative(iRow As Integer) As String
        Dim lPeerSize As Long = GetPeersSize()
        Dim sAddNode As String = KeyValue("addnode")
        If Len(sAddNode) = 0 And lPeerSize > 100 Then Return "PASSED (ADDNODE DOES NOT EXIST BUT PEERS ARE SYNCED)"
        If Len(sAddNode) > 4 And lPeerSize = 0 Then Return "PASSED (ADDNODE EXISTS BUT PEERS NOT SYNCED)"
        If lPeerSize > 100 Then Return "PASSED"
        If lPeerSize = 0 And sAddNode = "" Then Return "FAILED (Please addnode=node.gridcoin.us in config file)"
        Return "FAILED"
    End Function
    Private Sub VerifyCPIDIsValid(iRow As Integer)
        Dim sCPID As String = dvDiag.Rows(iRow - 1).Cells(1).Value
        Dim sError As String = ""
        Dim dTotalRac As Double = GetTotalCPIDRAC(sCPID, sError)
        If sError = "" Then sError = "PASSED"
        dvDiag.Rows(iRow).Cells(1).Value = sError
    End Sub
    Private Sub VerifyBoincPath(iRow As Integer)
        Dim dSz As Double = GetClientStateSize()
        If dSz > 1000 Then
            dvDiag.Rows(iRow).Cells(1).Value = "PASSED"
        Else
            dvDiag.Rows(iRow).Cells(1).Value = "Boinc Data dir not found.  Please set the 'datadir=' key to the boinc data path."
        End If
    End Sub
    Private Sub FindCPID(iRow As Integer)
        'First calculate the CPID
        Dim sCPID = ComputeLocalCPID()
        dvDiag.Rows(iRow).Cells(1).Value = sCPID
        msDiagnosticCPID = sCPID
    End Sub
    Private Sub VerifyClock(iRow As Integer)
        Dim d As New GRCDiagnostics
        Dim dtNIST As DateTime = d.GetNISTDateTime()
        Dim dtNowUTC As DateTime = DateTime.UtcNow
        Dim minDiff As Integer = DateDiff(DateInterval.Minute, dtNIST, dtNowUTC)
        Dim sResult As String
        If minDiff < 3 Then
            sResult = "PASSED"
        Else
            sResult = "Clock Off By " + Trim(minDiff) + " Minute(s).  Please update your system time."
        End If
        dvDiag.Rows(iRow).Cells(1).Value = sResult

    End Sub
    Private Sub VerifyTCP32749(iRow As Integer)
        Dim d As New GRCDiagnostics
        Dim bResult As Boolean = d.VerifyPort(32749, "node.gridcoin.us")
        Dim sResult As String = IIf(bResult = True, "PASSED", "FAILED")
        If sResult = "FAILED" Then sResult = "Outbound connection to 32749 failed.  Please try unblocking outbound traffic to 32749."
        dvDiag.Rows(iRow).Cells(1).Value = sResult
    End Sub
End Class
