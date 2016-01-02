Imports System.Windows.Forms

Public Class frmRain
    Public msProject As String = ""


    Private Sub frmRain_Load(sender As Object, e As System.EventArgs) Handles Me.Load
        Try

        Dim sHeading As String = "CPID;Address;Total RAC;Percent;Rain Amount"
        dgv.Rows.Clear()
        dgv.Columns.Clear()
        Dim vHeading() As String = Split(sHeading, ";")
        PopulateHeadings(vHeading, dgv, True)
        Dim iRow As Long = 0
        Me.Cursor.Current = Cursors.WaitCursor
        dgv.AutoResizeColumns(DataGridViewAutoSizeColumnsMode.ColumnHeader)
        dgv.ReadOnly = True
        dgv.EditingPanel.Visible = False
        lblHeading.Text = "Contribution for Project " + msProject

        'Get the RAC for the researchers for the Project
        Dim surrogateRow As New Row
        surrogateRow.Database = "CPID"
        surrogateRow.Table = "CPIDS"
        Dim lstCPIDs As List(Of Row) = GetList(surrogateRow, "*")
        Dim dTotalRAC As Double = 0
        lstCPIDs = GetList(surrogateRow, "*")
        For Each cpid As Row In lstCPIDs
            Dim dResearcherMagnitude As Double = 0
            Dim TotalRAC As Double = 0
            Dim TotalNetworkRAC As Double = 0
            Dim TotalMagnitude As Double = 0
            'Dim TotalRAC As Double = 0
            Dim surrogatePrjCPID As New Row
            surrogatePrjCPID.Database = "Project"
            surrogatePrjCPID.Table = msProject + "CPID"
            surrogatePrjCPID.PrimaryKey = msProject + "_" + cpid.PrimaryKey
            Dim rowRAC As Row = Read(surrogatePrjCPID)
            Dim dCPIDRAC As Double = Val(Trim("0" + rowRAC.RAC))
            If dCPIDRAC > 0 And cpid.DataColumn4.Length > 12 And cpid.PrimaryKey.Length > 12 Then
                dTotalRAC += dCPIDRAC
                dgv.Rows.Add()
                dgv.Rows(iRow).Cells(0).Value = cpid.PrimaryKey
                dgv.Rows(iRow).Cells(1).Value = cpid.DataColumn4
                dgv.Rows(iRow).Cells(2).Value = dCPIDRAC
                dgv.Rows(iRow).Cells(3).Value = 0
                dgv.Rows(iRow).Cells(4).Value = 0
                iRow += 1
                End If
            Next
        dgv.AllowUserToAddRows = False
        dgv.Rows.Add()
        Dim dTotalPercent As Double = 0
        Dim dTotalExpense As Double = 0
        'Now calculate Rain Amount
        For y As Integer = 0 To iRow - 1
            Dim dRainPercent As Double = 0
            Dim dRainAmount As Double = 0
            dRainPercent = Math.Round((Val(dgv.Rows(y).Cells(2).Value) / (dTotalRAC + 0.01)) * 100, 4)
            dRainAmount = Math.Round((dRainPercent / 100) * Val(txtRainAmount.Text), 4)
            dTotalExpense += dRainAmount
            dgv.Rows(y).Cells(3).Value = dRainPercent
            dgv.Rows(y).Cells(4).Value = dRainAmount
            dTotalPercent += dRainPercent
        Next
        If iRow = 0 Then Exit Sub

        'Total
        dgv.Rows(iRow).Cells(0).Value = "Total: " + Trim(iRow)
        dgv.Rows(iRow).Cells(3).Value = Math.Round(dTotalPercent, 4)
        dgv.Rows(iRow).Cells(4).Value = Math.Round(dTotalExpense, 4)

        Me.Cursor.Current = Cursors.Default

        Catch ex As Exception
            Me.Cursor.Current = Cursors.Default
        End Try

    End Sub

    Private Sub txtRainAmount_TextChanged(sender As System.Object, e As System.EventArgs) Handles txtRainAmount.TextChanged
        Timer1.Enabled = True 'Allow user to type for 3 more seconds before refreshing the page...

    End Sub

    Private Sub Timer1_Tick(sender As System.Object, e As System.EventArgs) Handles Timer1.Tick
        Timer1.Enabled = False
        Call frmRain_Load(Nothing, Nothing)
    End Sub

    Private Sub btnSave_Click(sender As System.Object, e As System.EventArgs) Handles btnSave.Click
        Dim dTotalExpense As Double = 0

        If dgv.Rows.Count < 4 Then
            MsgBox("Recipient list too small.", MsgBoxStyle.Critical)
            Exit Sub

        End If
        Dim sRow As String = ""
        Dim sSend As String = ""

        For y As Integer = 0 To dgv.Rows.Count - 2
            Dim dRainPercent As Double = 0
            Dim dRainAmount As Double = 0
            dRainAmount = Val(dgv.Rows(y).Cells(4).Value)
            dTotalExpense += dRainAmount
            Dim sAddress As String = dgv.Rows(y).Cells(1).Value
            sRow = sAddress + "<COL>" + Trim(dRainAmount) + "<ROW>"
            sSend += sRow
        Next
        If Len(sSend) > 8 Then sSend = Mid(sSend, 1, Len(sSend) - 5) 'Remove the last ROW delimiter

        Dim bInvalid As Boolean = False
        Dim dOrig As Double = Val(txtRainAmount.Text)
        If dTotalExpense > dOrig + 5 Or dTotalExpense < dOrig - 5 Then bInvalid = True
        If dTotalExpense < 1 Or bInvalid Then
            MsgBox("Rain amount is invalid.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        Dim sNarr As String = "Are you sure you would like to rain " + Trim(dTotalExpense) + " GRC?"
        Dim mbrResult As MsgBoxResult
        mbrResult = MsgBox(sNarr, MsgBoxStyle.YesNo, "Verification to Rain")
        If mbrResult = MsgBoxResult.Yes Then
            'Send the GRC to the lucky recipients
            Log("Raining " + Trim(sSend))

            Dim sResult As String = ExecuteRPCCommand("rain", sSend, "", "", "", "", "")
            MsgBox(sResult, MsgBoxStyle.Information, "Rain")
        End If
    End Sub
End Class
