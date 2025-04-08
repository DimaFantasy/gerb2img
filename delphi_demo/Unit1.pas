unit Unit1;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes, Vcl.Graphics,
  Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.StdCtrls, Vcl.ExtCtrls, System.JSON,
  Vcl.Mask;

type
  TForm1 = class(TForm)
    OpenDialog1: TOpenDialog;
    Image1: TImage;
    Panel1: TPanel;
    ButtonprocessGerberJSON: TButton;
    ButtonProcessGerber: TButton;
    CheckBoxDebug: TCheckBox;
    MaskEditDpi: TMaskEdit;
    procedure ButtonprocessGerberJSONClick(Sender: TObject);
    procedure ButtonProcessGerberClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
  private
    function GetErrorMessageByCode(code: Integer): string;
    function GetImageDPI: Double;
  public
  end;

var
  Form1: TForm1;

implementation

{$R *.dfm}

{$IFDEF CPUX64}
const
  GerberDll = 'gerb2img_x64.dll';
  GerberDebugDll = 'gerb2img_x64_debug.dll';
{$ELSE}
const
  GerberDll = 'gerb2img_x32.dll';
  GerberDebugDll = 'gerb2img_x32_debug.dll';
{$ENDIF}

// === ���������� ��� ������� � ����������� �� DLL ===

// ��� ������� ����������
function processGerberRelease(imageDPI: Double;
                              optGrowUnitsMillimeters: Boolean;
                              optBoarderUnitsMillimeters: Boolean;
                              optBoarder: Double;
                              optInvertPolarity: Boolean;
                              rowsPerStrip: Cardinal;
                              optGrowSize: Double;
                              optScaleX: Double;
                              optScaleY: Double;
                              outputFilename: PAnsiChar;
                              inputFilename: PAnsiChar): Integer; stdcall;
                              external GerberDll name 'processGerber';

function processGerberJSONRelease(const json: PAnsiChar): Integer; stdcall;
                              external GerberDll name 'processGerberJSON';

// ��� ���������� ����������
function processGerberDebug(imageDPI: Double;
                            optGrowUnitsMillimeters: Boolean;
                            optBoarderUnitsMillimeters: Boolean;
                            optBoarder: Double;
                            optInvertPolarity: Boolean;
                            rowsPerStrip: Cardinal;
                            optGrowSize: Double;
                            optScaleX: Double;
                            optScaleY: Double;
                            outputFilename: PAnsiChar;
                            inputFilename: PAnsiChar): Integer; stdcall;
                            external GerberDebugDll name 'processGerber';

function processGerberJSONDebug(const json: PAnsiChar): Integer; stdcall;
                            external GerberDebugDll name 'processGerberJSON';

// === ��������� DPI ===
function TForm1.GetImageDPI: Double;
begin
  if TryStrToFloat(MaskEditDpi.Text, Result) then
  begin
    if Result <= 0 then Result := 1024;
  end
  else
    Result := 1024;
end;

// === ��� ������ ===
function TForm1.GetErrorMessageByCode(code: Integer): string;
begin
  case code of
    0:    Result := '�������.';
    2:    Result := '���������� ������� ����.';
    3:    Result := '������ ��������� Gerber.';
    4:    Result := '������������ ���������.';
    5:    Result := '��� ����������� ��� ���������.';
    6:    Result := '������ ��������� ������.';
    7:    Result := '������ �������� ��������� �����.';
    8:    Result := '������ ��������� JSON.';
    9999: Result := '����������� ������.';
  else
    Result := '����������� ������.';
  end;
  Result := Result + ' (���: ' + IntToStr(code) + ')';
end;

// === ��������� Gerber (�������) ===
procedure TForm1.ButtonProcessGerberClick(Sender: TObject);
var
  inputFilePath, outputFilePath: string;
  resultCode: Integer;
begin
  if OpenDialog1.Execute then
  begin
    inputFilePath := OpenDialog1.FileName;
    outputFilePath := 'OUTPUT.bmp';

    // ������������� ������ � ����� "��������"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
      begin
        resultCode := processGerberDebug(
          GetImageDPI, False, False, 0, False, 512, 0, 1, 1,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end
      else
      begin
        resultCode := processGerberRelease(
          GetImageDPI, False, False, 0, False, 512, 0, 1, 1,
          PAnsiChar(AnsiString(outputFilePath)),
          PAnsiChar(AnsiString(inputFilePath))
        );
      end;

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('����������� ������� ���������!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // ���������� ������ � �������� ���������
      Screen.Cursor := crDefault;
    end;
  end;
end;

// === ��������� Gerber ����� JSON ===
procedure TForm1.ButtonprocessGerberJSONClick(Sender: TObject);
var
  jsonString, inputFilePath, escapedInputPath, outputFilePath: string;
  resultCode: Integer;
begin
  if OpenDialog1.Execute then
  begin
    inputFilePath := OpenDialog1.FileName;
    escapedInputPath := StringReplace(inputFilePath, '\', '\\', [rfReplaceAll]);
    outputFilePath := 'OUTPUT.bmp';

    jsonString := '{' + sLineBreak +
                  '  "imageDPI": ' + FloatToStr(GetImageDPI) + ',' + sLineBreak +
                  '  "optGrowUnitsMillimeters": false,' + sLineBreak +
                  '  "optBoarderUnitsMillimeters": false,' + sLineBreak +
                  '  "optBoarder": 0,' + sLineBreak +
                  '  "optInvertPolarity": true,' + sLineBreak +
                  '  "rowsPerStrip": 512,' + sLineBreak +
                  '  "optGrowSize": 0,' + sLineBreak +
                  '  "optScaleX": 1,' + sLineBreak +
                  '  "optScaleY": 1,' + sLineBreak +
                  '  "outputFilename": "' + outputFilePath + '",' + sLineBreak +
                  '  "inputFilename": "' + escapedInputPath + '"' + sLineBreak +
                  '}';

    // ������������� ������ � ����� "��������"
    Screen.Cursor := crHourGlass;

    try
      if CheckBoxDebug.Checked then
        resultCode := processGerberJSONDebug(PAnsiChar(AnsiString(jsonString)))
      else
        resultCode := processGerberJSONRelease(PAnsiChar(AnsiString(jsonString)));

      if resultCode = 0 then
      begin
        Image1.Picture.LoadFromFile(outputFilePath);
        ShowMessage('����������� ������� ���������!');
      end
      else
      begin
        ShowMessage(GetErrorMessageByCode(resultCode));
      end;
    finally
      // ���������� ������ � �������� ���������
      Screen.Cursor := crDefault;
    end;
  end;
end;


procedure TForm1.FormCreate(Sender: TObject);
begin
  MaskEditDpi.Text := '1024';
end;

end.

