defmodule Epad.Player do
  use Agent

  @max_players 3

  defstruct free: [], allocate: []

  def start_link(_opts) do
    Agent.start_link(&init_player/0, name: __MODULE__)
  end

  @spec start() :: integer() | :no_free_player
  def start() do
    Agent.get_and_update(__MODULE__, &new_player/1)
  end

  def stop(num) do
    Agent.update(__MODULE__, fn player ->
      allocate = List.delete_at(player.allocate, 0)
      free = [num | player.free]
      %{player | free: free, allocate: allocate}
    end)
  end

  defp init_player() do
    free = 0..@max_players |> Enum.to_list()
    %__MODULE__{free: free, allocate: []}
  end

  defp new_player(player) do
    {start_player, free} = List.pop_at(player.free, 0, :no_free_player)

    case start_player do
      :no_free_player ->
        {:no_free_player, player}

      _ ->
        allocate = [start_player | player.allocate]
        {start_player, %{player | free: free, allocate: allocate}}
    end
  end
end
