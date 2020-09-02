defmodule Epad.Devices.Device do
  use GenServer

  require Logger

  @epad_driver "epad_driver"
  @epad_uinput "/dev/uinput"

  # @epad_cmd_mask 0xF
  @epad_cmd_event 0x1

  @epad_profile_gamepad 0x1
  @epad_profile_keyboard 0x2
  @epad_profile_touchpad 0x3

  @epad_res_ok 0

  defstruct port: nil, profile: nil

  def child_spec(arg) do
    %{
      id: __MODULE__,
      start: {__MODULE__, :start_link, [arg]}
    }
  end

  def send_event(name, type, code, value) do
    event = "ev:#{type}:#{code}:#{value}"
    GenServer.cast(name, {:event, event})
  end

  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts)
  end

  # Callbacks

  @impl true
  def init(args) do
    profile = get_profile(Keyword.fetch!(args, :profile))
    res = :code.priv_dir(:epad) |> load_epad_driver()

    case res do
      :ok -> {:ok, %__MODULE__{profile: profile}, {:continue, :load}}
      {:error, _Reason} -> {:stop, :unknown}
    end
  end

  @impl true
  def handle_continue(:load, %__MODULE__{profile: profile} = state) do
    cmd = "#{@epad_driver} #{@epad_uinput} #{profile}"
    port = :erlang.open_port({:spawn_driver, cmd}, [])

    {:noreply, %{state | port: port}}
  end

  @impl true
  def handle_cast({:event, type_code_value}, %__MODULE__{port: port} = state) do
    :ok = event_port_control(port, @epad_cmd_event, type_code_value)

    {:noreply, state}
  end

  @impl true
  def terminate(_reason, %__MODULE__{}) do
    _ = unload_epad_driver()

    :ok
  end

  # private

  defp get_profile(:gamepad) do
    @epad_profile_gamepad
  end

  defp get_profile(:keyboard) do
    @epad_profile_keyboard
  end

  defp get_profile(:touchpad) do
    @epad_profile_touchpad
  end

  defp load_epad_driver({:error, _Reason} = error) do
    error
  end

  defp load_epad_driver(driver) do
    :erl_ddll.load(driver, @epad_driver |> to_charlist)
  end

  defp unload_epad_driver() do
    :erl_ddll.unload(@epad_driver |> to_charlist)
  end

  defp event_port_control(port, command, port_arg) do
    res_list = :erlang.port_control(port, command, port_arg)
    <<res_native::native-size(32)>> = :erlang.list_to_binary(res_list)
    convert_return_value(Bitwise.bsr(res_native, 24))
  end

  defp convert_return_value(bits) when bits === @epad_res_ok do
    :ok
  end

  defp convert_return_value(_) do
    :unknown_error
  end
end
