defmodule EpadWeb.GamepadChannel do
  use Phoenix.Channel

  alias Epad.Devices.Device
  alias Epad.Player
  alias Epad

  def join("epad:gamepad", %{}, socket) do
    device = Epad.gamepad() |> check_gamepad
    {:ok, assign(socket, :device, device)}
  end

  def handle_in("hello", _, socket) do
    socket =
      case Player.start() do
        :no_free_player ->
          push(socket, "hello", %{error: :no_free_player})
          socket

        player ->
          push(socket, "hello", %{inputId: player})
          assign(socket, :player, player)
      end

    {:noreply, socket}
  end

  def handle_in("event", %{"code" => code, "type" => type, "value" => value}, socket) do
    :ok = Device.send_event(socket.assigns[:device], type, code, value)
    {:noreply, socket}
  end

  def terminate(_reason, socket) do
    Player.stop(socket.assigns[:player])
  end

  defp check_gamepad({:ok, pid}) do
    pid
  end

  defp check_gamepad({:error, {:already_started, pid}}) do
    pid
  end
end
